!function() {
  var analytics = window.analytics = window.analytics || [];
  // The window.requestIdleCallback function is not available on all browsers
  // so we polyfill it here.
  idleCallback = window.requestIdleCallback ||
    function (cb) {
      var start = Date.now();
      return setTimeout(function () {
        cb({
          didTimeout: false,
          timeRemaining: function () {
            return Math.max(0, 50 - (Date.now() - start));
          }
        });
      }, 1);
    };

  if (!analytics.initialize) {
    if (analytics.invoked) {
      window.console && console.error && console.error('Segment snippet included twice.');
    } else {
      analytics.invoked = !0;
      analytics.methods = ['trackSubmit', 'trackClick', 'trackLink', 'trackForm', 'pageview', 'identify', 'reset', 'group', 'track', 'ready', 'alias', 'debug', 'page', 'once', 'off', 'on'];
      analytics.factory = function(t) {
        return function() {
          var e = Array.prototype.slice.call(arguments);
          e.unshift(t);
          analytics.push(e);
          return analytics;
        };
      };
      for (var t = 0; t < analytics.methods.length; t++) {
        var e = analytics.methods[t];
        analytics[e] = analytics.factory(e);
      }
      /*******************************************
       * PLEASE NOTE!!!!
       *******************************************/
      // They function defers the loading of segment until the browser is idle,
      // followed by waiting another 2 seconds. This significantly helps with page performance
      // because it defers loading of external resources.
      analytics.load = function(t, e) {
        idleCallback(() => {
          setTimeout(() => {
            var n = document.createElement('script');
            const domain = __SEGMENT_ANALYTICS_JS_DOMAIN__;
            n.type = 'text/javascript';
            n.async = !0;
            n.src = 'https://' + domain + '/analytics.js/v1/' + t + '/analytics.min.js';
            var a = document.getElementsByTagName('script')[0];
            a.parentNode.insertBefore(n, a);
            analytics._loadOptions = e;
          }, 2000);
        });
      };
      analytics.SNIPPET_VERSION = "4.1.0";
    }
  }
}();
