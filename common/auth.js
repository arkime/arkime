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

class Auth {
  static httpRealm;
  static passwordSecret;
  static passwordSecret256;
  static serverSecret256;
  static basePath;
  static authFunc;
  static requiredAuthHeader;
  static requiredAuthHeaderVal;
  static userAutoCreateTmpl;
  static regressionTests;

  static initialize (options) {
    Auth.debug = options.debug ?? 0;
    Auth.mode = options.mode ?? 'anonymous';
    Auth.basePath = options.basePath ?? '/';
    Auth.userNameHeader = options.userNameHeader;
    Auth.httpRealm = options.httpRealm ?? 'Moloch';
    Auth.passwordSecret = options.passwordSecret ?? 'password';
    Auth.passwordSecret256 = crypto.createHash('sha256').update(Auth.passwordSecret).digest();
    if (options.serverSecret) {
      Auth.serverSecret256 = crypto.createHash('sha256').update(options.serverSecret).digest();
    } else {
      Auth.serverSecret256 = Auth.passwordSecret256;
    }
    Auth.requiredAuthHeader = options.requiredAuthHeader;
    Auth.requiredAuthHeaderVal = options.requiredAuthHeaderVal;
    Auth.userAutoCreateTmpl = options.userAutoCreateTmpl;
    Auth.userAuthIps = new iptrie.IPTrie();
    Auth.regressionTests = false;

    if (options.userAuthIps) {
      for (const cidr in options.userAuthIps.split(',')) {
        const parts = cidr.split('/');
        if (parts[0].includes(':')) {
          Auth.userAuthIps.add(parts[0], +(parts[1] ?? 128), 1);
        } else {
          Auth.userAuthIps.add(`::ffff:${parts[0]}`, 96 + +(parts[1] ?? 32), 1);
        }
      }
    } else if (Auth.mode === 'header') {
      Auth.userAuthIps.add('::ffff:127.0.0.0', 96 + 8, 1);
      Auth.userAuthIps.add('::1', 128, 1);
    } else {
      Auth.userAuthIps.add('::', 0, 1);
    }

    if (Auth.mode === 'digest') {
      passport.use(new DigestStrategy({ qop: 'auth', realm: Auth.httpRealm },
        function (userid, done) {
          if (userid.startsWith('role:')) {
            console.log(`User ${userid} Can not authenticate with role`);
            return done('Can not authenticate with role');
          }
          User.getUserCache(userid, async (err, user) => {
            if (err) { return done(err); }
            if (!user) { console.log('User', userid, "doesn't exist"); return done(null, false); }
            if (!user.enabled) { console.log('User', userid, 'not enabled'); return done('Not enabled'); }

            await user.expandRoles();
            user.setLastUsed();
            return done(null, user, { ha1: Auth.store2ha1(user.passStore) });
          });
        },
        function (poptions, done) {
          // TODO:  Should check nonce here
          return done(null, true);
        }
      ));
    }

    switch (Auth.mode) {
    case 'anonymous':
      Auth.authFunc = Auth.anonymousAuth;
      break;
    case 'anonymousWithDB':
      Auth.authFunc = Auth.anonymousWithDBAuth;
      break;
    case 'digest':
      Auth.authFunc = Auth.digestAuth;
      break;
    case 'header':
      Auth.authFunc = Auth.headerAuth;
      break;
    case 'regressionTests':
      Auth.authFunc = Auth.regressionTestsAuth;
      Auth.regressionTests = true;
      break;
    }
  }

  static checkIps (req, res) {
    if (req.ip.includes(':')) {
      if (!Auth.userAuthIps.find(req.ip)) {
        res.status(403);
        res.send(JSON.stringify({ success: false, text: `Not allowed by ip (${req.ip})` }));
        console.log('Blocked by ip', req.ip, req.url);
        return 1;
      }
    } else {
      if (!Auth.userAuthIps.find(`::ffff:${req.ip}`)) {
        res.status(403);
        res.send(JSON.stringify({ success: false, text: `Not allowed by ip (${req.ip})` }));
        console.log('Blocked by ip', req.ip, req.url);
        return 1;
      }
    }

    return 0;
  }

  static anonymousAuth (req, res, next) {
    req.user = Object.assign(new User(), {
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
    req.user.expandRoles();
    return next();
  }

  static anonymousWithDBAuth (req, res, next) {
    req.user = Object.assign(new User(), {
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
    req.user.expandRoles();
    User.getUserCache('anonymous', (err, user) => {
      if (user) {
        req.user.setLastUsed();
        req.user.settings = user.settings;
        req.user.views = user.views;
        req.user.columnConfigs = user.columnConfigs;
        req.user.spiviewFieldConfigs = user.spiviewFieldConfigs;
        req.user.tableStates = user.tableStates;
        req.user.cont3xt = user.cont3xt;
      }
      return next();
    });
  }

  static regressionTestsAuth (req, res, next) {
    const userId = req.query.molochRegressionUser ?? 'anonymous';

    if (userId.startsWith('role:')) {
      return res.status(401).send(JSON.stringify({ success: false, text: 'Can not authenticate with role' }));
    }

    User.getUserCache(userId, (err, user) => {
      if (user) {
        req.user = user;
        req.user.setLastUsed();
        return next();
      }
      req.user = Object.assign(new User(), {
        userId: userId,
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
        req.user.roles = ['superAdmin'];
      } else if (userId === 'anonymous') {
        req.user.roles = ['arkimeAdmin', 'cont3xtUser', 'parliamentUser', 'usersAdmin', 'wiseUser'];
      } else {
        req.user.roles = ['arkimeUser', 'cont3xtUser', 'parliamentUser', 'wiseUser'];
      }

      req.user.expandRoles();
      return next();
    });
  }

  static digestAuth (req, res, next) {
    if (Auth.checkIps(req, res)) {
      return;
    }

    if (Auth.basePath !== '/') {
      req.url = req.url.replace('/', Auth.basePath);
    }
    passport.authenticate('digest', { session: false })(req, res, function (err) {
      if (Auth.basePath !== '/') {
        req.url = req.url.replace(Auth.basePath, '/');
      }
      if (err) {
        res.status(403);
        return res.send(JSON.stringify({ success: false, text: err }));
      } else {
        return next();
      }
    });
  }

  static headerAuth (req, res, next) {
    if (Auth.checkIps(req, res)) {
      return;
    }

    if (req.headers[Auth.userNameHeader] === undefined) {
      if (Auth.debug > 0) {
        console.log(`AUTH: looking for header ${Auth.userNameHeader} in the headers`, req.headers);
      }
      res.status(403);
      return res.send(JSON.stringify({ success: false, text: 'Username not found' }));
    }

    if (Auth.requiredAuthHeader !== undefined && Auth.requiredAuthHeaderVal !== undefined) {
      const authHeader = req.headers[Auth.requiredAuthHeader];
      if (authHeader === undefined) {
        return res.send('Missing authorization header');
      }
      let authorized = false;
      authHeader.split(',').forEach(headerVal => {
        if (headerVal.trim() === Auth.requiredAuthHeaderVal) {
          authorized = true;
        }
      });
      if (!authorized) {
        return res.send('Not authorized');
      }
    }

    const userId = req.headers[Auth.userNameHeader];
    if (userId === '') {
      return res.status(401).send(JSON.stringify({ success: false, text: 'User name header is empty' }));
    }

    if (userId.startsWith('role:')) {
      return res.status(401).send(JSON.stringify({ success: false, text: 'Can not authenticate with role' }));
    }

    async function headerAuthCheck (err, user) {
      if (err || !user) { return res.send(JSON.stringify({ success: false, text: 'User not found' })); }
      if (!user.enabled) { return res.send(JSON.stringify({ success: false, text: 'User not enabled' })); }
      if (!user.headerAuthEnabled) { return res.send(JSON.stringify({ success: false, text: 'User header auth not enabled' })); }

      req.user = user;
      await req.user.expandRoles();
      req.user.setLastUsed();
      return next();
    }

    User.getUserCache(userId, (err, user) => {
      if (Auth.userAutoCreateTmpl === undefined) {
        return headerAuthCheck(err, user);
      } else if ((err && err.toString().includes('Not Found')) || (!user)) { // Try dynamic creation
        const nuser = JSON.parse(new Function('return `' +
               Auth.userAutoCreateTmpl + '`;').call(req.headers));
        if (nuser.passStore === undefined) {
          nuser.passStore = Auth.pass2store(nuser.userId, crypto.randomBytes(48));
        }
        if (nuser.userId !== userId) {
          console.log(`WARNING - the userNameHeader (${Auth.userNameHeader}) said to use '${userId}' while the userAutoCreateTmpl returned '${nuser.userId}', reseting to use '${userId}'`);
          nuser.userId = userId;
        }
        if (nuser.userName === undefined || nuser.userName === 'undefined') {
          console.log(`WARNING - The userAutoCreateTmpl didn't set a userName, using userId for ${nuser.userId}`);
          nuser.userName = nuser.userId;
        }

        User.setUser(userId, nuser, (err, info) => {
          if (err) {
            console.log('Elastic search error adding user: (' + userId + '):(' + JSON.stringify(nuser) + '):' + err);
          } else {
            console.log('Added user:' + userId + ':' + JSON.stringify(nuser));
          }
          return User.getUserCache(userId, headerAuthCheck);
        });
      } else {
        return headerAuthCheck(err, user);
      }
    });
  }

  static s2sAuth (req, res, next) {
    const obj = Auth.auth2obj(req.headers['x-arkime-auth'] || req.headers['x-moloch-auth']);
    obj.path = obj.path.replace(Auth.basePath, '/');

    if (obj.user.startsWith('role:')) {
      return res.send('Can not authenticate with role');
    }

    if (obj.path !== req.url) {
      console.log('ERROR - mismatch url', obj.path, req.url);
      return res.send('Unauthorized based on bad url');
    }

    if (Math.abs(Date.now() - obj.date) > 120000) { // Request has to be +- 2 minutes
      console.log('ERROR - Denying server to server based on timestamp, are clocks out of sync?', Date.now(), obj.date);
      return res.send('Unauthorized based on timestamp - check that all Arkime viewer machines have accurate clocks');
    }

    // Don't look up user for receiveSession
    if (req.url.match(/^\/receiveSession/) || req.url.match(/^\/api\/sessions\/receive/)) {
      return next();
    }

    User.getUserCache(obj.user, (err, user) => {
      if (err) { return res.send('ERROR - x-arkime getUser - user: ' + obj.user + ' err:' + err); }
      if (!user) { return res.send(obj.user + " doesn't exist"); }
      if (!user.enabled) { return res.send(obj.user + ' not enabled'); }
      req.user = user;
      req.user.setLastUsed();
      return next();
    });
    return;
  }

  // Need this wrapper because app.use seems to save the value and not the array
  static doAuth (req, res, next) {
    return Auth.authFunc(req, res, next);
  }

  static md5 (str, encoding) {
    return crypto
      .createHash('md5')
      .update(str)
      .digest(encoding || 'hex');
  };

  // Hash (MD5) and encrypt the password before storing.
  // Encryption is used because ES is insecure by default and we don't want others adding accounts.
  static pass2store (userid, password) {
    // md5 is required because of http digest
    const m = Auth.md5(userid + ':' + Auth.httpRealm + ':' + password);

    // New style with IV: IV.E
    const iv = crypto.randomBytes(16);
    const c = crypto.createCipheriv('aes-256-cbc', Auth.passwordSecret256, iv);
    let e = c.update(m, 'binary', 'hex');
    e += c.final('hex');
    return iv.toString('hex') + '.' + e;
  };

  // Decrypt the encrypted hashed password, it is still hashed
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
        // eslint-disable-next-line node/no-deprecated-api
        const c = crypto.createDecipher('aes192', Auth.passwordSecret);
        let d = c.update(passstore, 'hex', 'binary');
        d += c.final('binary');
        return d;
      }
    } catch (e) {
      console.log("passwordSecret set in the [default] section can not decrypt information.  Make sure passwordSecret is the same for all nodes. You may need to re-add users if you've changed the secret.", e);
      process.exit(1);
    }
  };

  // Encrypt an object into an auth string
  static obj2auth (obj, secret) {
    // New style with IV: IV.E.H
    if (secret) {
      secret = crypto.createHash('sha256').update(secret).digest();
    } else {
      secret = Auth.serverSecret256;
    }

    const iv = crypto.randomBytes(16);
    const c = crypto.createCipheriv('aes-256-cbc', secret, iv);
    let e = c.update(JSON.stringify(obj), 'binary', 'hex');
    e += c.final('hex');
    e = iv.toString('hex') + '.' + e;
    const h = crypto.createHmac('sha256', secret).update(e).digest('hex');
    return e + '.' + h;
  };

  // Decrypt the auth string into an object
  static auth2obj (auth, secret) {
    const parts = auth.split('.');

    if (parts.length === 3) {
      // New style with IV: IV.E.H
      if (secret) {
        secret = crypto.createHash('sha256').update(secret).digest();
      } else {
        secret = Auth.serverSecret256;
      }

      const signature = Buffer.from(parts[2], 'hex');
      const h = crypto.createHmac('sha256', secret).update(parts[0] + '.' + parts[1]).digest();

      if (!crypto.timingSafeEqual(signature, h)) {
        throw new Error('Incorrect signature');
      }

      try {
        const c = crypto.createDecipheriv('aes-256-cbc', secret, Buffer.from(parts[0], 'hex'));
        let d = c.update(parts[1], 'hex', 'binary');
        d += c.final('binary');
        return JSON.parse(d);
      } catch (error) {
        console.log(error);
        throw new Error('Incorrect auth supplied');
      }
    } else {
      // Old style without IV: E or E.H

      secret = secret || Auth.serverSecret;

      if (parts.length === 1) {
        throw new Error('Missing signature');
      }

      if (parts.length > 1) {
        const signature = Buffer.from(parts[1], 'hex');
        const h = crypto.createHmac('sha256', secret).update(parts[0], 'hex').digest();

        if (!crypto.timingSafeEqual(signature, h)) {
          throw new Error('Incorrect signature');
        }
      }

      try {
        // eslint-disable-next-line node/no-deprecated-api
        const c = crypto.createDecipher('aes192', secret);
        let d = c.update(parts[0], 'hex', 'binary');
        d += c.final('binary');
        return JSON.parse(d);
      } catch (error) {
        console.log(error);
        throw new Error('Incorrect auth supplied');
      }
    }
  };

  static addS2SAuth (options, user, node, path, secret) {
    if (!options.headers) {
      options.headers = {};
    }
    options.headers['x-moloch-auth'] = Auth.obj2auth({
      date: Date.now(),
      user: user.userId,
      node: node,
      path: path
    }, secret);
  }
}

module.exports = Auth;

const User = require('../common/user');
