/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Snackbars"];

ChromeUtils.defineModuleGetter(this, "EventDispatcher", "resource://gre/modules/Messaging.jsm");

const LENGTH_INDEFINITE = -2;
const LENGTH_LONG = 0;
const LENGTH_SHORT = -1;

var Snackbars = {
  LENGTH_INDEFINITE: LENGTH_INDEFINITE,
  LENGTH_LONG: LENGTH_LONG,
  LENGTH_SHORT: LENGTH_SHORT,

  show: function(aMessage, aDuration, aOptions) {

    // Takes care of the deprecated toast calls
    if (typeof aDuration === "string") {
      [aDuration, aOptions] = migrateToastIfNeeded(aDuration, aOptions);
    }

    let msg = {
      type: "Snackbar:Show",
      message: aMessage,
      duration: aDuration,
    };

    if (aOptions && aOptions.backgroundColor) {
      msg.backgroundColor = aOptions.backgroundColor;
    }

    if (aOptions && aOptions.action) {
      msg.action = {};

      if (aOptions.action.label) {
        msg.action.label = aOptions.action.label;
      }

      EventDispatcher.instance.sendRequestForResult(msg)
        .then(result => aOptions.action.callback())
        .catch(result => {
          if (result === null) {
            /* The snackbar was dismissed without executing the callback, nothing to do here. */
          } else {
            Cu.reportError(result);
          }
        });
    } else {
      EventDispatcher.instance.sendRequest(msg);
    }
  },
};

function migrateToastIfNeeded(aDuration, aOptions) {
  let duration;
  if (aDuration === "long") {
    duration = LENGTH_LONG;
  } else {
    duration = LENGTH_SHORT;
  }

  let options = {};
  if (aOptions && aOptions.button) {
    options.action = {
      label: aOptions.button.label,
      callback: () => aOptions.button.callback(),
    };
  }
  return [duration, options];
}
