/******************************************************************************/
/* auth.js  -- common Auth apis
 *
 * Copyright Yahoo Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
'use strict';

// eslint-disable-next-line no-shadow
const crypto = require('crypto');
const passport = require('passport');
const DigestStrategy = require('passport-http').DigestStrategy;
const iptrie = require('iptrie');
const CustomStrategy = require('passport-custom');
const express = require('express');
const expressSession = require('express-session');
const uuid = require('uuid').v4;
const OIDC = require('openid-client');
const LRU = require('lru-cache');

class Auth {
  static debug;
  static mode;
  static regressionTests = false;
  static passwordSecret;
  static passwordSecret256;

  static #userNameHeader;
  static #serverSecret;
  static #serverSecret256;
  static #basePath;
  static #requiredAuthHeader;
  static #requiredAuthHeaderVal;
  static #userAutoCreateTmpl;
  static #userAuthIps;
  static #strategies;
  static #s2sRegressionTests;
  static #authRouter = express.Router();
  static #authConfig;
  static #passportAuthOptions = { session: false };
  static #caTrustCerts;
  static #passwordSecretSection;
  static #app;
  static #keyCache = new LRU({ max: 1000, maxAge: 1000 * 60 * 5 });

  // ----------------------------------------------------------------------------
  /**
   * Set up the app.use() calls for the express application. Call in chain where
   * auth should be installed.
   */
  static app (app, options) {
    Auth.#app = app;
    app.use(Auth.#authRouter);

    if (options?.doAuth !== false) {
      app.use(Auth.doAuth);
    }

    if (Auth.#app && Auth.#authConfig?.trustProxy !== undefined) {
      Auth.#doTrustProxy();
    }
  }

  // ----------------------------------------------------------------------------
  /**
   * Initialize the Auth subsystem
   * @param {boolean} options.debug=0 The debug level to use for Auth component
   * @param {string} options.mode=anonymous What auth mode to run in
   * @param {string} options.basePath=/ What the web base path is for the app
   * @param {string} options.userNameHeader In header auth mode, which http header has the user id
   * @param {string} options.passwordSecret=password For digest mode, what password to use to encrypt the password hash
   * @param {string} options.serverSecret=passwordSecret What password is used to encrypt S2S auth
   * @param {string} options.requiredAuthHeader In header auth mode, another header can be required
   * @param {string} options.requiredAuthHeaderVal In header auth mode, a comma separated list of values for requiredAuthHeader, if none are matched the user will not be authorized
   * @param {string} options.userAutoCreateTmpl A javascript string function that is used to create users that don't exist
   * @param {string} options.userAuthIps A comma separated list of CIDRs that users are allowed from
   * @param {boolean} options.s2s Support s2s auth also
   * @param {object} options.authConfig options specific to each auth mode
   * @param {object} options.caTrustFile Optional path to CA certificate file to use for external authentication
   */
  static initialize (options) {
    if (options.debug > 1) {
      console.log('Auth.initialize', options);
    }

    Auth.debug = options.debug ?? 0;
    Auth.mode = options.mode ?? 'anonymous';
    Auth.#basePath = options.basePath ?? '/';
    Auth.#userNameHeader = options.userNameHeader;
    Auth.#passwordSecretSection = options.passwordSecretSection ?? 'default';
    Auth.passwordSecret = options.passwordSecret ?? 'password';
    Auth.passwordSecret256 = crypto.createHash('sha256').update(Auth.passwordSecret).digest();
    if (options.serverSecret) {
      Auth.#serverSecret = options.serverSecret;
      Auth.#serverSecret256 = crypto.createHash('sha256').update(options.serverSecret).digest();
    } else {
      Auth.#serverSecret = options.passwordSecret;
      Auth.#serverSecret256 = Auth.passwordSecret256;
    }
    Auth.#requiredAuthHeader = options.requiredAuthHeader;
    Auth.#requiredAuthHeaderVal = options.requiredAuthHeaderVal;
    Auth.#userAutoCreateTmpl = options.userAutoCreateTmpl;
    Auth.#userAuthIps = new iptrie.IPTrie();
    Auth.#s2sRegressionTests = options.s2sRegressionTests;
    Auth.#authConfig = options.authConfig;
    Auth.#caTrustCerts = ArkimeUtil.certificateFileToArray(options.caTrustFile);

    if (Auth.#app && Auth.#authConfig?.trustProxy !== undefined) {
      Auth.#doTrustProxy();
    }

    if (options.userAuthIps) {
      for (const cidr of options.userAuthIps.split(',')) {
        const parts = cidr.split('/');
        if (parts[0].includes(':')) {
          Auth.#userAuthIps.add(parts[0], +(parts[1] ?? 128), 1);
        } else {
          Auth.#userAuthIps.add(`::ffff:${parts[0]}`, 96 + +(parts[1] ?? 32), 1);
        }
      }
    } else if (Auth.mode === 'header') {
      Auth.#userAuthIps.add('::ffff:127.0.0.0', 96 + 8, 1);
      Auth.#userAuthIps.add('::1', 128, 1);
    } else {
      Auth.#userAuthIps.add('::', 0, 1);
    }

    function check (field, str) {
      if (!ArkimeUtil.isString(Auth.#authConfig[field])) {
        console.log(`ERROR - ${str} missing from config file`);
        process.exit();
      }
    }

    let sessionAuth = false;
    switch (Auth.mode) {
    case 'anonymous':
      Auth.#strategies = ['anonymous'];
      break;
    case 'anonymousWithDB':
      Auth.#strategies = ['anonymousWithDB'];
      break;
    case 'digest':
      check('httpRealm');
      Auth.#strategies = ['digest'];
      break;
    case 'oidc':
      check('userIdField', 'authUserIdField');
      check('discoverURL', 'authDiscoverURL');
      check('clientId', 'authClientId');
      check('clientSecret', 'authClientSecret');
      check('redirectURIs', 'authRedirectURIs');
      Auth.#strategies = ['oidc'];
      sessionAuth = true;
      break;
    case 'header':
      Auth.#strategies = ['header', 'digest'];
      break;
    case 's2s':
      Auth.#strategies = ['s2s'];
      break;
    case 'regressionTests':
      Auth.#strategies = ['regressionTests'];
      Auth.regressionTests = true;
      break;
    default:
      console.log('ERROR - unknown authMode', ArkimeUtil.sanitizeStr(Auth.mode));
      process.exit(1);
    }

    if (options.s2s && !Auth.#strategies.includes('s2s')) {
      Auth.#strategies.unshift('s2s');
    }

    if (Auth.debug > 0) {
      console.log('AUTH strategies', Auth.#strategies);
    }

    Auth.#registerStrategies();

    // If sessionAuth is required enable the express and passport sessions
    if (sessionAuth) {
      Auth.#passportAuthOptions = { session: true, successRedirect: '/', failureRedirect: '/fail' };
      Auth.#authRouter.get('/fail', (req, res) => { res.send('User not found'); });
      Auth.#authRouter.use(expressSession({
        secret: uuid(),
        resave: false,
        saveUninitialized: true,
        cookie: { path: Auth.#basePath, secure: true, sameSite: Auth.#authConfig.cookieSameSite ?? 'Lax' }
      }));
      Auth.#authRouter.use(passport.initialize());
      Auth.#authRouter.use(passport.session());

      // only save the userId to passport session
      passport.serializeUser(function (user, done) {
        done(null, user.userId);
      });

      // load the user using the userid in the passport session
      passport.deserializeUser(function (userId, done) {
        User.getUserCache(userId, async (err, user) => {
          if (err) { return done('ERROR - passport-session getUser - user: ' + userId + ' err:' + err); }
          if (!user) { return done(userId + " doesn't exist"); }
          if (!user.enabled) { return done(userId + ' not enabled'); }

          user.setLastUsed();
          return done(null, user);
        });
      });
    } else {
      Auth.#authRouter.use(passport.initialize());
    }
  }

  // ----------------------------------------------------------------------------
  static #doTrustProxy () {
    const trustProxy = Auth.#authConfig.trustProxy;

    if (trustProxy === true || trustProxy === 'true') {
      Auth.#app.set('trust proxy', true);
    } else if (trustProxy === false || trustProxy === 'false') {
      Auth.#app.set('trust proxy', false);
    } else if (!isNaN(trustProxy)) {
      Auth.#app.set('trust proxy', parseInt(trustProxy));
    } else {
      Auth.#app.set('trust proxy', trustProxy);
    }
  }

  // ----------------------------------------------------------------------------
  static isAnonymousMode () {
    return Auth.mode === 'anonymous' || Auth.mode === 'anonymousWithDB';
  }

  // ----------------------------------------------------------------------------
  /* Register all the strategies that are supported */
  static async #registerStrategies () {
    // ----------------------------------------------------------------------------
    passport.use('anonymous', new CustomStrategy((req, done) => {
      const user = Object.assign(new User(), {
        userId: 'anonymous',
        enabled: true,
        webEnabled: true,
        headerAuthEnabled: false,
        emailSearch: true,
        removeEnabled: true,
        packetSearch: true,
        settings: {},
        welcomeMsgNum: 1,
        roles: ['superAdmin']
      });
      user.expandFromRoles();
      return done(null, user);
    }));

    // ----------------------------------------------------------------------------
    passport.use('anonymousWithDB', new CustomStrategy((req, done) => {
      // Setup anonymous user
      const user = Object.assign(new User(), {
        userId: 'anonymous',
        enabled: true,
        webEnabled: true,
        headerAuthEnabled: false,
        emailSearch: true,
        removeEnabled: true,
        packetSearch: true,
        settings: {},
        welcomeMsgNum: 1,
        roles: ['superAdmin']
      });
      user.expandFromRoles();

      User.getUserCache('anonymous', (err, dbUser) => {
        // Replace certain fields if available from db
        if (dbUser) {
          user.setLastUsed();
          user.settings = dbUser.settings;
          user.views = dbUser.views;
          user.columnConfigs = dbUser.columnConfigs;
          user.spiviewFieldConfigs = dbUser.spiviewFieldConfigs;
          user.tableStates = dbUser.tableStates;
          user.cont3xt = dbUser.cont3xt;
        }
        return done(null, user);
      });
    }));

    // ----------------------------------------------------------------------------
    passport.use('digest', new DigestStrategy({ qop: 'auth', realm: Auth.#authConfig.httpRealm }, (userId, done) => {
      if (userId.startsWith('role:')) {
        console.log(`AUTH: User ${userId} Can not authenticate with role`);
        return done('Can not authenticate with role');
      }
      User.getUserCache(userId, async (err, user) => {
        if (err) { return done(err); }
        if (!user) { console.log('AUTH: User', userId, "doesn't exist"); return done(null, false); }
        if (!user.enabled) { console.log('AUTH: User', userId, 'not enabled'); return done('Not enabled'); }

        user.setLastUsed();
        return done(null, user, { ha1: Auth.store2ha1(user.passStore) });
      });
    }, (poptions, done) => {
      return done(null, true);
    }));

    // ----------------------------------------------------------------------------
    passport.use('header', new CustomStrategy((req, done) => {
      if (Auth.#userNameHeader !== undefined && req.headers[Auth.#userNameHeader] === undefined) {
        if (Auth.debug > 0) {
          console.log(`AUTH: didn't find ${Auth.#userNameHeader} in the headers`, req.headers);
        }
        return done(null, false);
      }

      if (Auth.#requiredAuthHeader !== undefined && Auth.#requiredAuthHeaderVal !== undefined) {
        const authHeader = req.headers[Auth.#requiredAuthHeader];
        if (authHeader === undefined) {
          return done('Missing authorization header');
        }
        let authorized = false;
        authHeader.split(',').forEach(headerVal => {
          if (headerVal.trim() === Auth.#requiredAuthHeaderVal) {
            authorized = true;
          }
        });
        if (!authorized) {
          console.log(`The required auth header '${Auth.#requiredAuthHeader}' expected '${Auth.#requiredAuthHeaderVal}' and has `, ArkimeUtil.sanitizeStr(authHeader));
          return done('Bad authorization header');
        }
      }

      const userId = req.headers[Auth.#userNameHeader].trim();
      if (userId === '') {
        return done('User name header is empty');
      }

      if (userId.startsWith('role:')) {
        return done('Can not authenticate with role');
      }

      async function headerAuthCheck (err, user) {
        if (err || !user) { return done('User not found'); }
        if (!user.enabled) { return done('User not enabled'); }
        if (!user.headerAuthEnabled) { return done('User header auth not enabled'); }

        user.setLastUsed();
        return done(null, user);
      }

      User.getUserCache(userId, (err, user) => {
        if (Auth.#userAutoCreateTmpl === undefined) {
          return headerAuthCheck(err, user);
        } else if ((err && err.toString().includes('Not Found')) || (!user)) { // Try dynamic creation
          Auth.#dynamicCreate(userId, req.headers, headerAuthCheck);
        } else {
          return headerAuthCheck(err, user);
        }
      });
    }));

    // ----------------------------------------------------------------------------
    if (Auth.mode === 'oidc') {
      if (Auth.#caTrustCerts !== undefined) {
        OIDC.custom.setHttpOptionsDefaults({ ca: Auth.#caTrustCerts });
      }
      const issuer = await OIDC.Issuer.discover(Auth.#authConfig.discoverURL);
      const client = new issuer.Client({
        client_id: Auth.#authConfig.clientId,
        client_secret: Auth.#authConfig.clientSecret,
        redirect_uris: Auth.#authConfig.redirectURIs.split(','),
        token_endpoint_auth_method: 'client_secret_post'
      });

      passport.use('oidc', new OIDC.Strategy({
        client
      }, (tokenSet, userinfo, done) => {
        const userId = userinfo[Auth.#authConfig.userIdField];

        if (userId === undefined) {
          if (Auth.debug > 0) {
            console.log(`AUTH: didn't find ${Auth.#authConfig.userIdField} in the userinfo`, userinfo);
          }
          return done(null, false);
        }

        if (userId.startsWith('role:')) {
          console.log(`AUTH: User ${userId} Can not authenticate with role`);
          return done('Can not authenticate with role');
        }

        async function oidcAuthCheck (err, user) {
          if (err || !user) { return done('User not found'); }
          if (!user.enabled) { return done('User not enabled'); }
          if (!user.headerAuthEnabled) { return done('User header auth not enabled'); }

          user.setLastUsed();
          return done(null, user);
        }

        User.getUserCache(userId, (err, user) => {
          if (Auth.#userAutoCreateTmpl === undefined) {
            return oidcAuthCheck(err, user);
          } else if ((err && err.toString().includes('Not Found')) || (!user)) { // Try dynamic creation
            Auth.#dynamicCreate(userId, userinfo, oidcAuthCheck);
          } else {
            return oidcAuthCheck(err, user);
          }
        });
      }));
    }

    // ----------------------------------------------------------------------------
    passport.use('regressionTests', new CustomStrategy((req, done) => {
      const userId = req?.query?.molochRegressionUser ?? 'anonymous';
      if (userId.startsWith('role:')) {
        return done('Can not authenticate with role');
      }

      User.getUserCache(userId, (err, user) => {
        if (user) {
          user.setLastUsed();
          return done(null, user);
        }
        user = Object.assign(new User(), {
          userId,
          enabled: true,
          webEnabled: true,
          headerAuthEnabled: false,
          emailSearch: true,
          removeEnabled: true,
          packetSearch: true,
          settings: {},
          welcomeMsgNum: 1
        });

        if (userId === 'superAdmin') {
          user.roles = ['superAdmin'];
        } else if (userId === 'anonymous') {
          user.roles = ['arkimeAdmin', 'cont3xtUser', 'parliamentUser', 'usersAdmin', 'wiseUser'];
        } else {
          user.roles = ['arkimeUser', 'cont3xtUser', 'parliamentUser', 'wiseUser'];
        }

        user.expandFromRoles();
        return done(null, user);
      });
    }));

    // ----------------------------------------------------------------------------
    passport.use('s2s', new CustomStrategy((req, done) => {
      let obj = req.headers['x-arkime-auth'] ?? req.headers['x-moloch-auth'];

      if (obj === undefined) {
        return done(null, false);
      }

      try {
        if (Auth.#s2sRegressionTests) {
          obj = JSON.parse(obj);
        } else {
          obj = Auth.auth2obj(obj);
        }
      } catch (e) {
        console.log('AUTH: x-arkime-auth corrupt', e);
        return done('S2S auth header corrupt');
      }

      if (!ArkimeUtil.isString(obj.path)) {
        return done('S2S bad path');
      }

      if (!ArkimeUtil.isString(obj.user)) {
        return done('S2S bad user');
      }

      if (typeof (obj.date) !== 'number') {
        return done('S2S bad date');
      }

      if (obj.user.startsWith('role:')) {
        return done('Can not authenticate with role');
      }

      obj.path = obj.path.replace(Auth.#basePath, '/');
      if (obj.path !== req.url) {
        console.log('ERROR - mismatch url', obj.path, ArkimeUtil.sanitizeStr(req.url));
        return done('Unauthorized based on bad url');
      }

      if (Math.abs(Date.now() - obj.date) > 120000) { // Request has to be +- 2 minutes
        console.log('ERROR - Denying server to server based on timestamp, are clocks out of sync?', Date.now(), obj.date);
        return done('Unauthorized based on timestamp - check that all Arkime viewer machines have accurate clocks');
      }

      // Don't look up user for receiveSession
      if (req.url.match(/^\/receiveSession/) || req.url.match(/^\/api\/sessions\/receive/)) {
        return done(null, {});
      }

      // s2s && regressionTests for anonymous user we just fake
      if (Auth.regressionTests && obj.user === 'anonymous') {
        const user = Object.assign(new User(), {
          userId: obj.user,
          enabled: true,
          webEnabled: true,
          headerAuthEnabled: false,
          emailSearch: true,
          removeEnabled: true,
          packetSearch: true,
          settings: {},
          welcomeMsgNum: 1,
          roles: ['arkimeAdmin', 'cont3xtUser', 'parliamentUser', 'usersAdmin', 'wiseUser']
        });
        return done(null, user);
      }

      User.getUserCache(obj.user, async (err, user) => {
        if (err) { return done('ERROR - x-arkime getUser - user: ' + obj.user + ' err:' + err); }
        if (!user) { return done(obj.user + " doesn't exist"); }
        if (!user.enabled) { return done(obj.user + ' not enabled'); }

        user.setLastUsed();
        return done(null, user);
      });
    }));
  }

  // ----------------------------------------------------------------------------
  static #checkIps (req, res) {
    if (req.ip.includes(':')) {
      if (!Auth.#userAuthIps.find(req.ip)) {
        res.status(403);
        res.send(JSON.stringify({ success: false, text: `Not allowed by ip (${req.ip})` }));
        console.log('Blocked (userAuthIps setting) by ip', req.ip, req.url);
        return 1;
      }
    } else {
      if (!Auth.#userAuthIps.find(`::ffff:${req.ip}`)) {
        res.status(403);
        res.send(JSON.stringify({ success: false, text: `Not allowed by ip (${req.ip})` }));
        console.log('Blocked (userAuthIps setting) by ip', req.ip, req.url);
        return 1;
      }
    }

    return 0;
  }

  // ----------------------------------------------------------------------------
  static #dynamicCreate (userId, vars, cb) {
    if (Auth.debug > 0) {
      console.log('AUTH - #dynamicCreate', ArkimeUtil.sanitizeStr(userId));
    }
    const nuser = JSON.parse(new Function('return `' + Auth.#userAutoCreateTmpl + '`;').call(vars));
    if (nuser.passStore === undefined) {
      nuser.passStore = Auth.pass2store(nuser.userId, crypto.randomBytes(48));
    }
    if (nuser.userId !== userId) {
      console.log(`WARNING - the userNameHeader (${Auth.#userNameHeader}) said to use '${userId}' while the userAutoCreateTmpl returned '${nuser.userId}', reseting to use '${userId}'`);
      nuser.userId = userId;
    }
    if (nuser.userName === undefined || nuser.userName === 'undefined') {
      console.log(`WARNING - The userAutoCreateTmpl didn't set a userName, using userId for ${nuser.userId}`);
      nuser.userName = nuser.userId;
    }

    User.setUser(userId, nuser, (err, info) => {
      if (err) {
        console.log('OpenSearch/Elasticsearch error adding user: (%s):(%s):', userId, JSON.stringify(nuser), err);
      } else {
        console.log('Added user: %s:%s', userId, JSON.stringify(nuser));
      }
      return User.getUserCache(userId, cb);
    });
  }

  // ----------------------------------------------------------------------------
  static doAuth (req, res, next) {
    if (Auth.#checkIps(req, res)) {
      return;
    }

    if (typeof (req.isAuthenticated) === 'function' && req.isAuthenticated()) {
      return next();
    }

    if (Auth.#basePath !== '/') {
      req.url = req.url.replace('/', Auth.#basePath);
    }

    passport.authenticate(Auth.#strategies, Auth.#passportAuthOptions)(req, res, function (err) {
      if (Auth.#basePath !== '/') {
        req.url = req.url.replace(Auth.#basePath, '/');
      }
      if (err) {
        if (Auth.debug > 0) {
          console.log('AUTH: passport.authenticate fail', err);
        }
        res.status(403);
        return res.send(JSON.stringify({ success: false, text: err }));
      } else {
        return next();
      }
    });
  }

  // ----------------------------------------------------------------------------
  static md5 (str, encoding) {
    return crypto
      .createHash('md5')
      .update(str)
      .digest(encoding || 'hex');
  };

  // ----------------------------------------------------------------------------
  // Encrypt the hashed password for storing
  static ha12store (ha1) {
    // IV.E
    const iv = crypto.randomBytes(16);
    const c = crypto.createCipheriv('aes-256-cbc', Auth.passwordSecret256, iv);
    let e = c.update(ha1, 'binary', 'hex');
    e += c.final('hex');
    return iv.toString('hex') + '.' + e;
  }

  // ----------------------------------------------------------------------------
  // Hash (MD5) and encrypt the password before storing.
  // Encryption is used because OpenSearch/Elasticsearch is insecure by default and we don't want others adding accounts.
  static pass2store (userId, password) {
    // md5 is required because of http digest
    return Auth.ha12store(Auth.md5(userId + ':' + Auth.#authConfig.httpRealm + ':' + password));
  };

  // ----------------------------------------------------------------------------
  // Decrypt the encrypted hashed password, it is still hashed
  // Support 2 styles of decryption
  static store2ha1 (passstore) {
    try {
      const parts = passstore.split('.');
      if (parts.length === 2) {
        // New style with IV: IV.E
        const c = crypto.createDecipheriv('aes-256-cbc', Auth.passwordSecret256, Buffer.from(parts[0], 'hex'));
        let d = c.update(parts[1], 'hex', 'binary');
        d += c.final('binary');
        return d;
      } else {
        // Old style without IV: E
        // eslint-disable-next-line n/no-deprecated-api
        const c = crypto.createDecipher('aes192', Auth.passwordSecret);
        let d = c.update(passstore, 'hex', 'binary');
        d += c.final('binary');
        return d;
      }
    } catch (e) {
      console.log(`passwordSecret set in the [${Auth.#passwordSecretSection}] section can not decrypt information.  Make sure passwordSecret is the same for all nodes/applications. You may need to re-add users if you've changed the secret.`, e);
      process.exit(1);
    }
  };

  // ----------------------------------------------------------------------------
  // Encrypt an object into an auth string
  static obj2authNext (obj, secret) {
    secret ??= Auth.#serverSecret;

    let entry, key, salt;
    if ((entry = Auth.#keyCache.get(secret))) {
      salt = entry.salt;
      key = entry.key;
    } else {
      salt = crypto.randomBytes(16);
      key = crypto.pbkdf2Sync(secret, salt, 300000, 32, 'sha256');
      Auth.#keyCache.set(secret, { key, salt });
      Auth.#keyCache.set(`${secret}:${salt.toString('hex')}`, key);
    }

    const iv = crypto.randomBytes(12);
    const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);

    let data = cipher.update(JSON.stringify(obj), 'utf8', 'hex');
    data += cipher.final('hex');

    const tag = cipher.getAuthTag();

    const auth = {
      iv: iv.toString('hex'),
      salt: salt.toString('hex'),
      data, // already hex
      tag: tag.toString('hex')
    };

    return JSON.stringify(auth);
  }

  // ----------------------------------------------------------------------------
  // Encrypt an object into an auth string
  // IV.E.H
  static obj2auth (obj, secret) {
    // HACK: Remove in future, for cookies use Next since local
    if (obj.pid !== undefined) { return Auth.obj2authNext(obj, secret); }

    if (secret) {
      secret = crypto.createHash('sha256').update(secret).digest();
    } else {
      secret = Auth.#serverSecret256;
    }

    const iv = crypto.randomBytes(16);
    const c = crypto.createCipheriv('aes-256-cbc', secret, iv);
    let e = c.update(JSON.stringify(obj), 'utf8', 'hex');
    e += c.final('hex');
    e = iv.toString('hex') + '.' + e;
    const h = crypto.createHmac('sha256', secret).update(e).digest('hex');
    return e + '.' + h;
  };

  // ----------------------------------------------------------------------------
  // Decrypt the auth string into an object
  static auth2objNext (auth, secret) {
    secret ??= Auth.#serverSecret;
    try {
      const { iv, salt, data, tag } = JSON.parse(auth);

      let key = Auth.#keyCache.get(`${secret}:${salt}`);
      if (!key) {
        key = crypto.pbkdf2Sync(secret, Buffer.from(salt, 'hex'), 300000, 32, 'sha256');
        Auth.#keyCache.set(`${secret}:${salt}`, key);
      }

      const decipher = crypto.createDecipheriv('aes-256-gcm', key, Buffer.from(iv, 'hex'));
      decipher.setAuthTag(Buffer.from(tag, 'hex'));

      let decrypted = decipher.update(data, 'hex', 'utf8');
      decrypted += decipher.final('utf8');

      return JSON.parse(decrypted);
    } catch (error) {
      console.log(error);
      throw new Error('Incorrect auth supplied');
    }
  }

  // ----------------------------------------------------------------------------
  // Decrypt the auth string into an object
  // IV.E.H
  static auth2obj (auth, secret) {
    if (auth[0] === '{') { return Auth.auth2objNext(auth, secret); }
    const parts = auth.split('.');

    if (parts.length !== 3) {
      throw new Error(`Unsupported auth2obj ${parts.length}`);
    }

    if (secret) {
      secret = crypto.createHash('sha256').update(secret).digest();
    } else {
      secret = Auth.#serverSecret256;
    }

    const signature = Buffer.from(parts[2], 'hex');
    const h = crypto.createHmac('sha256', secret).update(parts[0] + '.' + parts[1]).digest();

    if (!crypto.timingSafeEqual(signature, h)) {
      throw new Error('Incorrect signature');
    }

    try {
      const c = crypto.createDecipheriv('aes-256-cbc', secret, Buffer.from(parts[0], 'hex'));
      let d = c.update(parts[1], 'hex', 'utf8');
      d += c.final('utf8');
      return JSON.parse(d);
    } catch (error) {
      console.log(error);
      throw new Error('Incorrect auth supplied');
    }
  };

  // ----------------------------------------------------------------------------
  static addS2SAuth (options, user, node, path, secret) {
    if (!options.headers) {
      options.headers = {};
    }
    options.headers['x-arkime-auth'] = Auth.obj2auth({
      date: Date.now(),
      user: user.userId,
      node,
      path
    }, secret);
  }
}

// ----------------------------------------------------------------------------
module.exports = Auth;

const User = require('../common/user');
const ArkimeUtil = require('../common/arkimeUtil');
