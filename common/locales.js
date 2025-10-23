/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

const fs = require('fs');
const path = require('path');

/**
 * Handler for loading and serving locale files
 * @param {Object} req - Express request object
 * @param {Object} res - Express response object
 */
function getLocales(req, res) {
  const localesPath = path.join(__dirname, 'vueapp/locales');

  try {
    const files = fs.readdirSync(localesPath);
    const localeFiles = files.filter(file => file.endsWith('.json'));
    const locales = {};

    for (const file of localeFiles) {
      const localeCode = file.replace('.json', '');
      const localeFilePath = path.join(localesPath, file);

      try {
        const localeData = JSON.parse(fs.readFileSync(localeFilePath, 'utf8'));

        // Validate that the file has proper structure
        if (localeData.__meta && localeData.__meta.code && localeData.__meta.name) {
          locales[localeCode] = localeData;
        } else {
          console.warn(`Invalid locale file format: ${file}`);
        }
      } catch (error) {
        console.error(`Error parsing locale file ${file}:`, error);
      }
    }

    res.json({ success: true, locales });
  } catch (error) {
    console.error('Error reading locales directory:', error);
    res.status(500).json({ success: false, error: 'Failed to read locales' });
  }
}

module.exports = {
  getLocales
};
