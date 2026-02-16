/******************************************************************************/
/* auth.js  -- common Auth apis
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

// eslint-disable-next-line no-shadow
const crypto = require('crypto');
const passport = require('passport');
const DigestStrategy = require('passport-http').DigestStrategy;
const BasicStrategy = require('passport-http').BasicStrategy;
const iptrie = require('arkime-iptrie');
const CustomStrategy = require('passport-custom');
const LocalStrategy = require('passport-local');
const express = require('express');
const expressSession = require('express-session');
const OIDC = require('openid-client');
const { LRUCache } = require('lru-cache');
const bodyParser = require('body-parser');

class Auth {
  static mode;
  static regressionTests = false;
  static passwordSecret;
  static passwordSecret256;

  static #userNameHeader;
  static #appAdminRole;
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
  static #keyCache = new LRUCache({ max: 1000, ttl: 1000 * 60 * 5 });
  static #logoutUrl;
  static #logoutUrlMethod;

  // ----------------------------------------------------------------------------
  /**
   * Set up the app.use() calls for the express application. Call in chain where
   * auth should be installed.
   */
  static app (app, options) {
    Auth.#app = app;
    app.use(Auth.#ppChecker);
    app.use(Auth.#authRouter);

    app.post('/api/login', bodyParser.urlencoded({ extended: true }));
    app.use(Auth.doAuth);

    if (Auth.#app && Auth.#authConfig?.trustProxy !== undefined) {
      Auth.#doTrustProxy();
    }
  }

  // ----------------------------------------------------------------------------
  /**
   * Initialize the Auth subsystem
   * @param {string} options.mode=digest What auth mode to run in
   * @param {string} options.basePath=/ What the web base path is for the app
   * @param {string} options.userNameHeader In header auth mode, which http header has the user id
   * @param {string} options.passwordSecret=password For basic/digest mode, what password to use to encrypt the password hash
   * @param {string} options.serverSecret=passwordSecret What password is used to encrypt S2S auth
   * @param {string} options.requiredAuthHeader In header auth mode, another header can be required
   * @param {string} options.requiredAuthHeaderVal In header auth mode, a comma separated list of values for requiredAuthHeader, if none are matched the user will not be authorized
   * @param {string} options.userAutoCreateTmpl A javascript string function that is used to create users that don't exist
   * @param {boolean} options.s2s Support s2s auth also
   * @param {object} options.authConfig options specific to each auth mode
   * @param {object} options.caTrustFile Optional path to CA certificate file to use for external authentication
   */
  static initialize (options) {
    // Make sure all options we need below are set
    options ??= {};
    options.mode ??= ArkimeConfig.get('authMode');
    options.userNameHeader ??= ArkimeConfig.get('userNameHeader');
    options.passwordSecret ??= ArkimeConfig.getFull(options.passwordSecretSection ?? undefined, 'passwordSecret');
    options.serverSecret ??= ArkimeConfig.get('serverSecret');
    options.requiredAuthHeader ??= ArkimeConfig.get('requiredAuthHeader');
    options.requiredAuthHeaderVal ??= ArkimeConfig.get('requiredAuthHeaderVal');
    options.userAutoCreateTmpl ??= ArkimeConfig.get('userAutoCreateTmpl');
    options.userAuthIps = ArkimeConfig.getArray('userAuthIps');
    options.caTrustFile ??= ArkimeConfig.get('caTrustFile');

    options.authConfig ??= {};
    options.authConfig.httpRealm ??= ArkimeConfig.get('httpRealm', 'Moloch');
    options.authConfig.userIdField ??= ArkimeConfig.get('authUserIdField');
    options.authConfig.discoverURL ??= ArkimeConfig.get('authDiscoverURL');
    options.authConfig.clientId ??= ArkimeConfig.get('authClientId');
    options.authConfig.clientSecret ??= ArkimeConfig.get('authClientSecret');
    options.authConfig.redirectURIs ??= ArkimeConfig.get('authRedirectURIs');
    options.authConfig.trustProxy ??= ArkimeConfig.get('authTrustProxy');
    options.authConfig.cookieSameSite ??= ArkimeConfig.get('authCookieSameSite');
    options.authConfig.cookieSecure ??= ArkimeConfig.get('authCookieSecure', true);
    options.authConfig.oidcScope ??= ArkimeConfig.get('authOIDCScope', 'openid');
    options.authConfig.jwsAlgorithm ??= ArkimeConfig.get('authJwsAlgorithm', 'RS256');

    if (ArkimeConfig.debug > 1) {
      console.log('Auth.initialize', options);
    }

    if (options.mode === undefined) {
      if (options.userNameHeader) {
        if (options.userNameHeader.match(/^(digest|basic|anonymous|oidc|basic\+oidc|form|basic\+form)$/)) {
          console.log(`WARNING - Using authMode=${options.userNameHeader} setting since userNameHeader set, add to config file to silence this warning.`);
          options.mode = options.userNameHeader;
          delete options.userNameHeader;
        } else {
          console.log('WARNING - Using authMode=header since not set, add to config file to silence this warning.');
          options.mode = 'header';
        }
      } else {
        console.log('WARNING - Using authMode=digest since not set, add to config file to silence this warning.');
        options.mode = 'digest';
      }
    }

    Auth.mode = options.mode;
    Auth.#userNameHeader = options.userNameHeader;
    Auth.#appAdminRole = options.appAdminRole;
    Auth.#basePath = options.basePath ?? '/';
    Auth.#passwordSecretSection = options.passwordSecretSection ?? 'default';
    Auth.passwordSecret = options.passwordSecret ?? 'password';
    Auth.passwordSecret256 = crypto.createHash('sha256').update(Auth.passwordSecret).digest();
    if (options.serverSecret) {
      Auth.#serverSecret = options.serverSecret;
      Auth.#serverSecret256 = crypto.createHash('sha256').update(options.serverSecret).digest();
    } else {
      Auth.#serverSecret = Auth.passwordSecret;
      Auth.#serverSecret256 = Auth.passwordSecret256;
    }
    Auth.#requiredAuthHeader = options.requiredAuthHeader;
    Auth.#requiredAuthHeaderVal = options.requiredAuthHeaderVal?.split(',').map(s => s.trim()).filter(s => s !== '');
    Auth.#userAutoCreateTmpl = options.userAutoCreateTmpl;
    Auth.#userAuthIps = new iptrie.IPTrie();
    Auth.#s2sRegressionTests = options.s2sRegressionTests;
    Auth.#authConfig = options.authConfig;
    Auth.#caTrustCerts = ArkimeUtil.certificateFileToArray(options.caTrustFile);

    if (Auth.#app && Auth.#authConfig?.trustProxy !== undefined) {
      Auth.#doTrustProxy();
    }

    if (options.userAuthIps) {
      for (const cidr of options.userAuthIps) {
        const parts = cidr.split('/');
        try {
          if (parts[0].includes(':')) {
            Auth.#userAuthIps.add(parts[0], +(parts[1] ?? 128), 1);
          } else {
            Auth.#userAuthIps.add(`::ffff:${parts[0]}`, 96 + +(parts[1] ?? 32), 1);
          }
        } catch (e) {
          console.log('ERROR - userAuthIps setting contains bad IP or cidr', cidr);
          process.exit(1);
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

    Auth.#logoutUrl = ArkimeConfig.get('logoutUrl');
    Auth.#logoutUrlMethod = ArkimeConfig.get('logoutUrlMethod', 'POST');
    const addBasic = Auth.mode.startsWith('basic+');
    if (addBasic) {
      Auth.mode = Auth.mode.slice(6);
    }

    let sessionAuth = false;
    switch (Auth.mode) {
    case 'anonymous':
      Auth.#strategies = ['anonymousWithDB'];
      break;
    case 'basic':
      check('httpRealm');
      Auth.#strategies = ['basic'];
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
      Auth.#strategies = ['oidc'];
      Auth.#passportAuthOptions = { session: true, failureRedirect: `${Auth.#basePath}api/login`, scope: Auth.#authConfig.oidcScope };
      sessionAuth = true;
      break;
    case 'form':
      Auth.#strategies = ['form'];
      Auth.#passportAuthOptions = { session: true, failureRedirect: `${Auth.#basePath}auth` };
      sessionAuth = true;
      Auth.#logoutUrl ??= `${Auth.#basePath}logout`;
      break;
    case 'headerOnly':
      Auth.#strategies = ['header'];
      break;
    case 'header':
    case 'header+digest':
      Auth.#strategies = ['header', 'digest'];
      break;
    case 'header+basic':
      Auth.#strategies = ['header', 'basic'];
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

    // Add basic to beginning
    if (addBasic) {
      Auth.#strategies.unshift('basic');
    }

    // Add s2s to beginning
    if (options.s2s && !Auth.#strategies.includes('s2s')) {
      Auth.#strategies.unshift('s2s');
    }

    if (ArkimeConfig.debug > 0) {
      console.log('AUTH strategies', Auth.#strategies);
    }

    Auth.#registerStrategies();

    // If sessionAuth is required enable the express and passport sessions
    if (sessionAuth) {
      Auth.#authRouter.use(expressSession({
        name: 'ARKIME-SID',
        secret: Auth.passwordSecret + Auth.#serverSecret,
        resave: false,
        saveUninitialized: true,
        cookie: { path: Auth.#basePath, secure: Auth.#authConfig.cookieSecure, sameSite: Auth.#authConfig.cookieSameSite ?? 'Lax', maxAge: 24 * 60 * 60 * 1000, httpOnly: true },
        store: new ESStore({ })
      }));
      Auth.#authRouter.use(passport.initialize());
      Auth.#authRouter.use(passport.session());

      if (Auth.mode === 'form') {
        const fs = require('fs');
        const path = require('path');

        Auth.#authRouter.get('/logo.png', (req, res) => {
          res.sendFile(path.join(__dirname, '../assets/Arkime_Logo_Mark_FullGradient.png'));
        });

        Auth.#authRouter.get('/auth', (req, res) => {
          // User is not authenticated, show the login form
          let html = fs.readFileSync(path.join(__dirname, '/vueapp/formAuth.html'), 'utf-8');
          html = html.toString().replace(/@@BASEHREF@@/g, Auth.#basePath)
            .replace(/@@MESSAGE@@/g, ArkimeConfig.get('loginMessage', ''))
            .replace(/@@OGURL@@/g, req.session.ogurl ?? Auth.#basePath);
          return res.send(html);
        });

        Auth.#authRouter.post('/logout', (req, res, next) => {
          req.logout((err) => {
            if (err) { return next(err); }
            return res.redirect(`${Auth.#basePath}auth`);
          });
        });
      }

      // only save the userId to passport session
      passport.serializeUser(function (user, done) {
        done(null, user.userId);
      });

      // load the user using the userId in the passport session
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
  static get appAdminRole () {
    return Auth.#appAdminRole;
  }

  // ----------------------------------------------------------------------------
  static logoutUrl (req) {
    let logoutUrl = Auth.#logoutUrl;
    if (req.session?.id_token !== undefined) {
      logoutUrl = logoutUrl.replace('ARKIME_ID_TOKEN', req.session.id_token);
    }
    if (ArkimeConfig.debug > 0) {
      console.log('Set logoutUrl to', req.user.userId, '=>', Auth.#logoutUrl);
    }
    return logoutUrl;
  }

  // ----------------------------------------------------------------------------
  static get logoutUrlMethod () {
    return Auth.#logoutUrlMethod;
  }

  // ----------------------------------------------------------------------------
  /**
   * Checks a user's permission to access a resource to update/delete
   * only allow admins, editors, or creator can update/delete resources
   * @param {function} dbFunc The function to call to get the resource
   * @param {string} ownerProperty The property on the resource that contains the owner
   */
  static checkResourceAccess (dbFunc, ownerProperty) {
    return async (req, res, next) => {
      if (req.user.hasRole(Auth.appAdminRole)) { // an admin can do anything
        return next();
      } else {
        try {
          const id = req.params.id ?? req.params.key;
          if (!id) {
            return res.serverError(404, 'Missing resource id');
          }

          let resource = await dbFunc(id);
          if (resource?.body?._source) {
            resource = resource.body._source;
          }

          if (!resource) {
            return res.serverError(404, 'Unknown resource');
          }

          const settingUser = req.settingUser || req.user;
          if ( // and creator or editor can update resources
            (resource[ownerProperty] && resource[ownerProperty] === settingUser.userId) ||
            settingUser.hasRole(resource.editRoles)
          ) {
            return next();
          }

          return res.serverError(403, 'Permission denied');
        } catch (err) {
          if (ArkimeConfig.debug > 0) {
            console.log('ERROR - checking resource access:', err);
          }
          return res.serverError(404, 'Unknown resource');
        }
      }
    };
  }

  // ----------------------------------------------------------------------------
  /* Register all the strategies that are supported */
  static async #registerStrategies () {
    // ----------------------------------------------------------------------------
    passport.use('anonymous', new CustomStrategy(async (req, done) => {
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
      await user.expandFromRoles();
      return done(null, user);
    }));

    // ----------------------------------------------------------------------------
    passport.use('anonymousWithDB', new CustomStrategy(async (req, done) => {
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
      await user.expandFromRoles();

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
        } else {
          user.save();
        }
        return done(null, user);
      });
    }));

    // ----------------------------------------------------------------------------
    passport.use('basic', new BasicStrategy((userId, password, done) => {
      if (userId.startsWith('role:')) {
        console.log(`AUTH: User ${userId} Can not authenticate with role`);
        return done('Can not authenticate with role');
      }
      User.getUserCache(userId, async (err, user) => {
        if (err) { return done(err); }
        if (!user) { console.log('AUTH: User', userId, "doesn't exist"); return done(null, false); }
        if (!user.enabled) { console.log('AUTH: User', userId, 'not enabled'); return done('Not enabled'); }

        const storeHa1 = Auth.store2ha1(user.passStore, user.userId);
        const ha1 = Auth.pass2ha1(userId, password);
        if (storeHa1.length !== ha1.length || !crypto.timingSafeEqual(Buffer.from(storeHa1), Buffer.from(ha1))) return done(null, false);

        user.setLastUsed();
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
        return done(null, user, { ha1: Auth.store2ha1(user.passStore, user.userId) });
      });
    }, (poptions, done) => {
      return done(null, true);
    }));

    // ----------------------------------------------------------------------------
    passport.use('header', new CustomStrategy((req, done) => {
      if (Auth.#userNameHeader !== undefined && req.headers[Auth.#userNameHeader] === undefined) {
        if (ArkimeConfig.debug > 0) {
          console.log(`AUTH: didn't find ${Auth.#userNameHeader} in the headers`, req.headers);
        }
        return done(null, false);
      }

      if (Auth.#requiredAuthHeader !== undefined && Auth.#requiredAuthHeaderVal !== undefined) {
        const authHeader = req.headers[Auth.#requiredAuthHeader];
        if (authHeader === undefined) {
          return done('Missing authorization header');
        }
        const authorized = authHeader.split(',').some(headerVal => Auth.#requiredAuthHeaderVal.includes(headerVal.trim()));
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

        await user.updateDynamicRoles(req.headers);
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

      // User didn't set a logoutUrl, so we will use the end_session_endpoint from the issuer
      if (Auth.#logoutUrl === undefined && issuer.end_session_endpoint !== undefined) {
        if (Auth.#authConfig.redirectURIs) {
          const logoutUrl = new URL(issuer.end_session_endpoint);
          logoutUrl.searchParams ??= new URLSearchParams();
          logoutUrl.searchParams.set('id_token_hint', 'ARKIME_ID_TOKEN');
          logoutUrl.searchParams.set('post_logout_redirect_uri', Auth.#authConfig.redirectURIs.split(',')[0].replace(/\/auth\/login\/callback$/, '/auth/logout/callback'));
          Auth.#logoutUrl = logoutUrl.toString();
          Auth.#logoutUrlMethod = 'GET';
        } else {
          console.log('WARNING - No redirectURIs set in authConfig, logoutUrl will not work');
        }
      }

      const client = new issuer.Client({
        client_id: Auth.#authConfig.clientId,
        client_secret: Auth.#authConfig.clientSecret,
        redirect_uris: Auth.#authConfig.redirectURIs ? Auth.#authConfig.redirectURIs.split(',') : undefined,
        token_endpoint_auth_method: 'client_secret_post',
        id_token_signed_response_alg: Auth.#authConfig.jwsAlgorithm
      });

      passport.use('oidc', new OIDC.Strategy({
        client
      }, (tokenSet, userinfo, done) => {
        const userId = userinfo[Auth.#authConfig.userIdField];

        if (userId === undefined) {
          if (ArkimeConfig.debug > 0) {
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

          await user.updateDynamicRoles(userinfo);
          user.setLastUsed();
          return done(null, user, { id_token: tokenSet.id_token });
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
    passport.use('form', new LocalStrategy((userId, password, done) => {
      if (userId.startsWith('role:')) {
        console.log(`AUTH: User ${userId} Can not authenticate with role`);
        return done('Can not authenticate with role');
      }
      User.getUserCache(userId, async (err, user) => {
        if (err) { return done(err); }
        if (!user) { console.log('AUTH: User', userId, "doesn't exist"); return done(null, false); }
        if (!user.enabled) { console.log('AUTH: User', userId, 'not enabled'); return done('Not enabled'); }

        const storeHa1 = Auth.store2ha1(user.passStore, user.userId);
        const ha1 = Auth.pass2ha1(userId, password);
        if (storeHa1.length !== ha1.length || !crypto.timingSafeEqual(Buffer.from(storeHa1), Buffer.from(ha1))) return done(null, false);

        user.setLastUsed();
        return done(null, user);
      });
    }));

    // ----------------------------------------------------------------------------
    passport.use('regressionTests', new CustomStrategy((req, done) => {
      const userId = req?.query?.arkimeRegressionUser ?? 'anonymous';
      if (userId.startsWith('role:')) {
        return done('Can not authenticate with role');
      }

      User.getUserCache(userId, (err, user) => {
        if (user) {
          user.setLastUsed();
        }
        return done(null, user);
      });
    }));

    // ----------------------------------------------------------------------------
    passport.use('s2s', new CustomStrategy(async (req, done) => {
      let obj = req.headers['x-arkime-auth'];

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

      let objPath = obj.path;
      if (Auth.#basePath.length > 1 && obj.path.startsWith(Auth.#basePath)) {
        objPath = obj.path.substring(Auth.#basePath.length);
      }
      if (obj.path !== req.url && objPath !== req.url) {
        console.log('ERROR - mismatch url object:', obj.path, 'request:', ArkimeUtil.sanitizeStr(req.url));
        return done('Unauthorized based on bad url');
      }

      if (Math.abs(Date.now() - obj.date) > 120000) { // Request has to be +- 2 minutes
        console.log('ERROR - Denying server to server based on timestamp, are clocks out of sync?', Date.now(), obj.date);
        return done('Unauthorized based on timestamp - check that all Arkime viewer machines have accurate clocks');
      }

      // Don't look up user for receiveSession
      if (req.url.match(/^\/receiveSession/i) || req.url.match(/^\/api\/sessions\/receive/i)) {
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
        await user.expandFromRoles();
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
    if (req.ip === undefined) {
      return 0;
    }

    if (req.ip.includes(':')) {
      if (!Auth.#userAuthIps.find(req.ip)) {
        res.status(403);
        res.json({ success: false, text: `Not allowed by ip (${req.ip})` });
        console.log('Blocked (userAuthIps setting) by ip', req.ip, req.url);
        return 1;
      }
    } else {
      if (!Auth.#userAuthIps.find(`::ffff:${req.ip}`)) {
        res.status(403);
        res.json({ success: false, text: `Not allowed by ip (${req.ip})` });
        console.log('Blocked (userAuthIps setting) by ip', req.ip, req.url);
        return 1;
      }
    }

    return 0;
  }

  // ----------------------------------------------------------------------------
  static #dynamicCreate (userId, vars, cb) {
    if (ArkimeConfig.debug > 0) {
      console.log('AUTH - #dynamicCreate', ArkimeUtil.sanitizeStr(userId));
    }
    const nuser = JSON.parse(new Function('return `' + Auth.#userAutoCreateTmpl + '`;').call(vars));
    if (nuser.passStore === undefined) {
      nuser.passStore = Auth.pass2store(nuser.userId, crypto.randomBytes(48));
    }

    if (nuser.userId !== userId) {
      if (nuser.userId === undefined) {
        if (ArkimeConfig.debug > 0) {
          console.log(`WARNING - the userAutoCreateTmpl didn't set a userId field, instead using header/oidc set '${userId}'`);
        }
      } else {
        console.log(`WARNING - the userAutoCreateTmpl set userId to a different value than header/oidc '${userId}' while the userAutoCreateTmpl returned '${nuser.userId}', resetting to use '${userId}'`);
      }
      nuser.userId = userId;
    }

    if (nuser.userName === undefined || nuser.userName === 'undefined') {
      console.log(`WARNING - The userAutoCreateTmpl didn't set a userName, using userId ${nuser.userId} for userName`);
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
  static #ppChecker (req, res, next) {
    if (req.path.match(/\/(__proto__|constructor)/)) {
      return ArkimeUtil.serverError.call(res, 403, 'Bad path ' + ArkimeUtil.safeStr(req.path));
    }

    if (!req.query) { return next(); }

    for (const key in req.query) {
      if (ArkimeUtil.isPP(req.query[key])) {
        return ArkimeUtil.serverError.call(res, 403, 'Invalid value for ' + ArkimeUtil.safeStr(key));
      }
    }

    return next();
  }

  // ----------------------------------------------------------------------------
  static doAuth (req, res, next) {
    if (Auth.#checkIps(req, res)) {
      return;
    }

    if (req._parsedUrl.pathname === '/auth/logout/callback') {
      if (req.session) {
        req.session.destroy((err) => {});
      }
      res.cookie('ARKIME-SID', '', { maxAge: 0, path: Auth.#basePath });
      res.redirect(Auth.#basePath);
      return;
    }

    if (typeof (req.isAuthenticated) === 'function' && req.isAuthenticated()) {
      return next();
    }

    if (Auth.#basePath !== '/') {
      req.url = req.url.replace('/', Auth.#basePath);
    }

    if (req.url.toLowerCase() !== '/api/login' && req.originalUrl !== '/' && req.session) {
      // save the original url so we can redirect after successful login
      // the ogurl is saved in the form login page and accessed using req.body.ogurl
      req.session.ogurl = Buffer.from(Auth.obj2authNext(req.originalUrl)).toString('base64');
    }

    const passportAuthOptionsExtra = {};
    if (Auth.#strategies.includes('oidc') && (Auth.#authConfig.redirectURIs === undefined || Auth.#authConfig.redirectURIs.split(',').length > 1)) {
      passportAuthOptionsExtra.redirect_uri = req.protocol + '://' + req.hostname + `${Auth.#basePath}auth/login/callback`;
    }

    passport.authenticate(Auth.#strategies, { ...Auth.#passportAuthOptions, ...passportAuthOptionsExtra })(req, res, function (err) {
      if (req.session !== undefined && req.authInfo?.id_token !== undefined) {
        req.session.id_token ??= req.authInfo.id_token;
      }

      if (Auth.#basePath !== '/') {
        req.url = req.url.replace(Auth.#basePath, '/');
      }
      if (err) {
        if (ArkimeConfig.debug > 0) {
          console.log('AUTH: passport.authenticate fail', err);
        }
        res.status(403);
        return res.json({ success: false, text: err });
      } else {
        // Redirect to / if this is a login url
        if (req.route?.path === '/api/login' || req._parsedUrl.pathname === '/auth/login/callback') {
          if (req.body?.ogurl) {
            try {
              const ogurl = Auth.auth2objNext(Buffer.from(req.body.ogurl, 'base64').toString());
              return res.redirect(ogurl);
            } catch (e) {
              console.log('Error', e);
              // Fall through to redirect below
            }
          }
          return res.redirect(Auth.#basePath);
        } else if (req._parsedUrl.pathname === '/auth/logout/callback') {
        }
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
  }

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
  // Hash (MD5) the password
  static pass2ha1 (userId, password) {
    return Auth.md5(userId + ':' + Auth.#authConfig.httpRealm + ':' + password);
  }

  // ----------------------------------------------------------------------------
  // Hash (MD5) and encrypt the password before storing.
  // Encryption is used because OpenSearch/Elasticsearch is insecure by default and we don't want others adding accounts.
  static pass2store (userId, password) {
    // md5 is required because of http digest
    return Auth.ha12store(Auth.pass2ha1(userId, password));
  }

  // ----------------------------------------------------------------------------
  // Decrypt the encrypted hashed password, it is still hashed
  // Support 2 styles of decryption
  static store2ha1 (passstore, userId) {
    try {
      const parts = passstore.split('.');
      if (parts.length === 2) {
        // IV.E
        const c = crypto.createDecipheriv('aes-256-cbc', Auth.passwordSecret256, Buffer.from(parts[0], 'hex'));
        let d = c.update(parts[1], 'hex', 'binary');
        d += c.final('binary');
        return d;
      } else {
        console.log(`WARNING - user '${userId}' passStore is using old encryption style, please reset user's password to update`);
        return '';
      }
    } catch (e) {
      console.log(`passwordSecret set in the [${Auth.#passwordSecretSection}] section can not decrypt '${userId}' information.  Make sure passwordSecret is the same for all nodes/applications. You may need to re-add users or reset passwords if you've changed the secret.`, e);
      return '';
    }
  }

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
  }

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
  static auth2obj (auth, secret) {
    // New json style
    if (auth[0] === '{') { return Auth.auth2objNext(auth, secret); }

    // New json style, but still encoded, bad proxy probably
    if (auth.startsWith('%7B%22')) { return Auth.auth2objNext(decodeURIComponent(auth), secret); }

    // IV.E.H
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
  }

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

  // ----------------------------------------------------------------------------
  // express middleware to set req.settingUser to who to work on, depending if admin or not
  // This returns fresh from db
  static getSettingUserDb (req, res, next) {
    let userId;

    if (!req.query.userId || req.query.userId === req.user.userId) {
      if (Auth.regressionTests) {
        req.settingUser = req.user;
        return next();
      }

      userId = req.user.userId;
    } else if (!req.user.hasRole('usersAdmin') || (!req.url.toLowerCase().startsWith('/api/user/password') && Auth.#appAdminRole && !req.user.hasRole(Auth.#appAdminRole))) {
      // user is trying to get another user's settings without admin privilege
      return res.serverError(403, 'Need admin privileges');
    } else {
      userId = req.query.userId;
    }

    User.getUser(userId, function (err, user) {
      if (err || !user) {
        if (!Auth.passwordSecret) {
          req.settingUser = JSON.parse(JSON.stringify(req.user));
          delete req.settingUser.found;
        } else {
          return res.serverError(403, 'Unknown user');
        }

        return next();
      }

      req.settingUser = user;
      return next();
    });
  }
}

// ----------------------------------------------------------------------------
class ESStore extends expressSession.Store {
  static #client;
  static #index;
  static #ttl = 24 * 60 * 60 * 1000; // 24 hrs

  constructor (options) {
    super();
    setTimeout(async () => {
      ESStore.#client = User.getClient();
      ESStore.start();
    }, 100);
    const prefix = ArkimeUtil.formatPrefix(ArkimeConfig.get('usersPrefix', ArkimeConfig.get('prefix', 'arkime_')));
    ESStore.#index = `${prefix}sids_v50`;
  }

  // ----------------------------------------------------------------------------
  static async start () {
    // Create Index if needed
    try {
      await ESStore.#client.indices.create({
        index: ESStore.#index,
        body: {
          settings: {
            number_of_shards: 1,
            number_of_replicas: 0,
            auto_expand_replicas: '0-1'
          }
        }
      });
    } catch (err) {
      // If already exists ignore error
      if (err.meta.body?.error?.type !== 'resource_already_exists_exception') {
        console.log(err);
        process.exit(0);
      }
    }

    // Update mapping if needed
    await ESStore.#client.indices.putMapping({
      index: ESStore.#index,
      body: {
        dynamic_templates: [
          {
            string_template: {
              match_mapping_type: 'string',
              mapping: {
                type: 'keyword'
              }
            }
          }
        ],
        properties: {
          _timestamp: { type: 'date' }
        }
      }
    });

    // Delete old sids, ignore errors
    try {
      await ESStore.#client.deleteByQuery({
        index: ESStore.#index,
        body: {
          query: {
            range: {
              _timestamp: {
                lte: new Date().getTime() - ESStore.#ttl
              }
            }
          }
        },
        timeout: '5m'
      });
    } catch (err) { }
  }

  // ----------------------------------------------------------------------------
  destroy (sid, callback) {
    ESStore.#client.delete({
      index: ESStore.#index,
      id: sid
    }, (err, response) => {
      callback();
    });
  }

  // ----------------------------------------------------------------------------
  get (sid, callback) {
    ESStore.#client.get({
      index: ESStore.#index,
      id: sid
    }, (err, response) => {
      if (err) {
        if (response?.statusCode === 404) {
          return callback();
        }
        return callback(err);
      }

      const _source = response.body._source;
      if (new Date().getTime() - _source._timestamp > ESStore.#ttl) {
        this.destroy(sid, () => {});
        return callback();
      }
      return callback(err, _source);
    });
  }

  // ----------------------------------------------------------------------------
  set (sid, session, callback) {
    session._timestamp = new Date().getTime();
    ESStore.#client.index({
      index: ESStore.#index,
      id: sid,
      body: session
    }, (err, response) => {
      callback(err);
    });
  }
}
// ----------------------------------------------------------------------------
module.exports = Auth;

const User = require('../common/user');
const ArkimeUtil = require('../common/arkimeUtil');
const ArkimeConfig = require('../common/arkimeConfig');
