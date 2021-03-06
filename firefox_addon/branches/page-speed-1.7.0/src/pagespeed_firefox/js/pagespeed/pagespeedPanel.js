/**
 * Copyright 2008-2009 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @fileoverview Creates a Firebug panel for Page Speed.
 *
 * @author Tony Gentilcore
 */

FBL.ns(function() { with (FBL) {

/** Base URL of the latency best practices document. */
var BASE_LATENCY_DOC_HREF_ =
    'http://code.google.com/speed/page-speed/docs/';

// TODO: move these to an actual stylesheet, once we come up with an
// easy way to drop a new stylesheet into panel.html.
var CSS_STYLE_SCORE_SIZER_ =
    ['position: relative',
     'width: 14px',
     'height: 14px',
     'overflow: hidden',
     'margin: auto',
     'margin-top: 2px'].join(';');

var CSS_STYLE_SCORE_SIZER_OVERALL_ =
    [CSS_STYLE_SCORE_SIZER_,
     'display: inline-block',
     'vertical-align: text-top'].join(';');

var CSS_STYLE_SCORE_RED_ =
    ['position: absolute',
     'left: 0px',
     'top: 0px'].join(';');

var CSS_STYLE_SCORE_YELLOW_ =
    ['position: absolute',
     'left: -14px',
     'top: 0px'].join(';');

var CSS_STYLE_SCORE_GREEN_ =
    ['position: absolute',
     'left: -28px',
     'top: 0px'].join(';');

var CSS_STYLE_SCORE_INFO_ =
    ['position: absolute',
     'left: -42px',
     'top: 0px'].join(';');

var generateIconHtml = function(iconStyle, sizerStyle, titleText) {
  return DIV({'class': 'netLabel', 'style': sizerStyle},
             IMG({'style': iconStyle,
                  'src': 'chrome://pagespeed/content/scoreIcon.png',
                  'title': titleText}
                )
            );
};

/**
 * @param {number} colorCode One of PAGESPEED.Utils.SCORE_CODE_*.
 * @return {string} The CSS style for the given color code.
 */
var getStyleForColorCode = function(colorCode) {
  switch (colorCode) {
    case PAGESPEED.Utils.SCORE_CODE_RED:
      return CSS_STYLE_SCORE_RED_;
    case PAGESPEED.Utils.SCORE_CODE_YELLOW:
      return CSS_STYLE_SCORE_YELLOW_;
    case PAGESPEED.Utils.SCORE_CODE_GREEN:
      return CSS_STYLE_SCORE_GREEN_;
    case PAGESPEED.Utils.SCORE_CODE_INFO:
      return CSS_STYLE_SCORE_INFO_;
  }
  return CSS_STYLE_SCORE_INFO_;
};

/**
 * @extends {Firebug.Panel}
 * @constructor
 */
function PageSpeedPanel() {}
PageSpeedPanel.prototype = domplate(Firebug.Panel, {
  // Unique identifier for this panel.
  name: 'pagespeed',

  // Text to display in the panel's tab.
  title: 'Page Speed',

  // Indicates that the content of this panel is not editable.
  editable: false,

  // Initial domplate that is displayed when the panel is opened.
  welcomePageTag:
      DIV({'class': 'moduleManagerBox',
           'style': 'padding: 0 10px;'},
          H1({'class': 'moduleManagerHead'},
             'Page Speed'
            ),
          INPUT({'type': 'button', 'value': 'Analyze Performance',
                 'onclick': '$analyzePerformance'}),
          P({'style': 'padding-top:5px'},
            'See the ',
            A({'href': 'http://code.google.com/' +
                       'speed/page-speed/docs/rules_intro.html',
              'onclick': '$openLink'},
              'Page Speed documentation'
              ),
            ' for detailed information on the rules used to evaluate web pages.'
            ),
          P({'class': 'moduleManagerDecription'},
            'Page Speed Copyright &copy; 2010 Google Inc.'
           ),
          DIV({'style': 'padding-top:5px'},
              FOR('dep', '$dependencies',
                  P('Page Speed has not been tested with the version ' +
                    'of $dep.name currently running in Firefox.<br/>' +
                    'If you encounter problems, please install $dep.name ' +
                    'version $dep.maximumVersion'
                   )
                 )
             )
         ),

  // Main domplate for the rules view.
  // params: overallStyle, overallSummary
  tableTag: TABLE({'class': 'netTable', 'cellpadding': '0', 'cellspacing': '0',
                   'onclick': '$showDetails', 'style': 'font-size:13px'},
                  TBODY(
                    TR({'class': 'netRow netSummaryRow'},
                       TD({'class': 'netCol',
                           'colspan': '3',
                           'style': 'padding: 0 10px;'},
                          'Page Speed Score:' +
                          '&nbsp;$overallScore/100&nbsp;&nbsp;',
                          generateIconHtml(
                              '$overallStyle',
                              CSS_STYLE_SCORE_SIZER_OVERALL_,
                              '$overallSummary'),
                          '&nbsp;&nbsp;',
                          INPUT({'type': 'button', 'value': 'Refresh Analysis',
                                'onclick': '$analyzePerformance'})
                         )
                      )
                  )
                 ),

  // Domplate for each individual rule.
  // params: rules[rule{name, score, href, hasDetails, details,
  //                    hasHeading, heading}].
  ruleTag: FOR('rule', '$rules',
               TR({'class': 'netRow netRow.loaded',
                   '$hasHeaders': '$rule.hasDetails',
                   '$loaded': true},
                  TD({'class': 'netCol', 'width': '50'},
                     generateIconHtml(
                         '$rule.scoreStyle',
                         CSS_STYLE_SCORE_SIZER_,
                         '$rule.score')
                    ),
                  TD({'class': 'netCol', 'width': '16'},
                     DIV({'class': 'netHrefLabel netLabel',
                          'style': 'display:block;margin-top:2px;height:16px;'}
                        )
                    ),
                  TD({'class': 'netSizeCol netCol', 'width': '*',
                      'style': 'text-align:left'},
                     DIV({'class': 'netSizeLabel netLabel'},
                         A({'href': '$rule.href',
                            'style': 'z-index:100;position:relative;',
                            'title': 'Learn More',
                            'onclick': '$openLink'
                           }, '$rule.name')
                        )
                    )
                 )
              ),


  /**
   * Creates an object suitable to fill the tableTag domplate.
   *
   * @param {number} overallScoreColorCode The overall score color
   *     code (one of PAGESPEED.Utils.SCORE_CODE_*).
   * @return {Object} The domplate arguments object.
   */
  createTableTagDomplateData: function(overallScoreColorCode, overallScore) {
    var overallSummary = '';
    switch (overallScoreColorCode) {
      case PAGESPEED.Utils.SCORE_CODE_RED:
        overallSummary = 'Significant performance improvements are possible';
        break;
      case PAGESPEED.Utils.SCORE_CODE_YELLOW:
        overallSummary = 'Moderate performance improvements are possible';
        break;
      case PAGESPEED.Utils.SCORE_CODE_GREEN:
        overallSummary =
            'This site follows the Page Speed performance ' +
            'guidelines. Nice job!';
        break;
    }
    return {
      'overallStyle': getStyleForColorCode(overallScoreColorCode),
      'overallSummary': overallSummary,
      'overallScore': overallScore
    };
  },

  /**
   * Creates an object suitable to fill the ruleTag domplate.
   *
   * @param {PAGESPEED.LintRule} oRule The LintRule to create ruleTagData for.
   * @return {Object} The domplate arguments object.
   */
  createRuleTagDomplateData: function(oRule) {
    var details = (oRule.warnings || '');
    if (oRule.information) {
      if (details.length > 0 ||
          PAGESPEED.Utils.getColorCode(oRule) !=
              PAGESPEED.Utils.SCORE_CODE_INFO) {
        details += '<p><u>Non-scoring information</u></p>';
      }
      details += oRule.information;
    }
    var scoreStyle = getStyleForColorCode(PAGESPEED.Utils.getColorCode(oRule));

    return {'name': oRule.name,
            'score': (isNaN(oRule.score) ?
                          oRule.score :
                          'Score: ' + Math.round(oRule.score) + '/100'),
            'href': BASE_LATENCY_DOC_HREF_ + oRule.href,
            'hasDetails': !!details,
            'details': details,
            'scoreStyle': scoreStyle
           };
  },

  // Domplate for the expanded view of a rule's details.
  detailsTag: TR({'class': 'netInfoRow'},
                 TD({'colspan': '3',
                     'class': 'netInfoCol'},
                    DIV({'class': 'netInfoBody',
                         'style': 'border-bottom:1px solid #efefef'})
                   )
                ),

  // Domplate which displays a progress bar.
  // params: width, text, subtext
  progressTag: DIV({'style': 'margin-left:100px;margin-top:100px'},
                   DIV({'style':
                        'border:1px solid black;height:20px;width:200px;'},
                       DIV({'style':
                            'background:#bbb;height:20px;width:$width'})
                      ),
                   DIV({}, '$text'),
                   DIV({}, '$subtext')
                  ),

  // Main domplate for the resources view.
  // params: totalComponents, totalFileSize, totalTransferSize
  componentsTag: TABLE({'class': 'netTable', 'cellpadding': '0',
                        'cellspacing': '0', 'onclick': '$showHeaders'},
                   TBODY(
                    TR({'class': 'netRow netSummaryRow'},
                       TD({'class': 'netCol'}, ''),
                       TD({'class': 'netCol',
                           'style': 'text-align:center',
                           'colspan': 2}, 'Status'),
                       TD({'class': 'netCol'}, 'Type'),
                       TD({'class': 'netCol'}, 'File Size'),
                       TD({'class': 'netCol'}, 'Transfer Size')
                      ),
                    TR({'class': 'netRow netSummaryRow'},
                       TD({'class': 'netCol'}, ''),
                       TD({'class': 'netCol'}, ''),
                       TD({'class': 'netCol'}, ''),
                       TD({'class': 'netCol'}, '$totalComponents'),
                       TD({'class': 'netCol'}, '$totalFileSize'),
                       TD({'class': 'netCol'}, '$totalTransferSize')
                      )
                   )
                 ),

  // Domplate which displays the resources tab.
  componentTag: FOR('component', '$components',
               TR({'class': 'netRow netRow.loaded',
                   '$hasHeaders': 'true',
                   '$loaded': true},
                  TD({'class': 'netHrefCol netCol', 'width': '50%'},
                     DIV({'class': 'netHrefLabel netLabel',
                          'style': 'max-width:45%'}, '$component.path'),
                     DIV({'class': 'netFullHrefLabel netHrefLabel netLabel'},
                       A({'href': '$component.href', 'target': '_blank'},
                         '$component.href'))
                    ),
                  TD({'class': 'netSizeCol netCol', 'width': '5%',
                      'style': 'text-align:right'},
                     DIV({'class': 'netSizeLabel netLabel'},
                         '$component.statusCode')
                    ),
                  TD({'class': 'netSizeCol netCol', 'width': '5%',
                      'style': 'text-align:left'},
                     DIV({'class': 'netSizeLabel netLabel'},
                         '$component.fromCache')
                    ),
                  TD({'class': 'netSizeCol netCol', 'width': '16%',
                      'style': 'text-align:left'},
                     DIV({'class': 'netSizeLabel netLabel'}, '$component.type')
                    ),
                  TD({'class': 'netSizeCol netCol', 'width': '12%',
                      'style': 'text-align:left'},
                     DIV({'class': 'netSizeLabel netLabel'},
                         '$component.filesize')
                    ),
                  TD({'class': 'netSizeCol netCol', 'width': '12%',
                      'style': 'text-align:left'},
                     DIV({'class': 'netSizeLabel netLabel'},
                         '$component.xfersize')
                    )
                 )
              ),

  // Domplate for the expanded view of a resource's headers.
  headersTag: TR({'class': 'netInfoRow'},
                 TD({'colspan': '7',
                     'class': 'netInfoCol'},
                    DIV({'class': 'netInfoBody',
                         'style': 'border-bottom:1px solid #efefef'})
                   )
                ),

  // Domplate for a blank div that is useful for writing static HTML to.
  // TODO: Remove this when we remove services.js
  blankTag: DIV({'style': 'font-size:13px'}),

  /**
   * Called after module.initialize
   * @override
   */
  initializeNode: function(myPanelNode) {
    PAGESPEED.PageSpeedContext.panel = this;
    PAGESPEED.curButtonId = '';

    // Find mismatched dependencies.
    var mismatchedDependencies = [];
    for (var addonName in PAGESPEED.DEPENDENCIES) {
      var dependency = PAGESPEED.DEPENDENCIES[addonName];
      if (!PAGESPEED.Utils.dependencyIsSatisfied(dependency)) {
        dependency.name = addonName;
        mismatchedDependencies.push(dependency);
      }
    }

    // Install a function on the document that toggles the display of the
    // element with the given id.
    this.document.toggleView = function(doc, id) {
      var elem = doc.getElementById(id);
      elem.style.display = elem.style.display == 'none' ? 'block' : 'none';
    };

    // Install a function on the document that opens a link in a new window
    // or tab.
    this.document.openLink = function(anchorElem) {
      PAGESPEED.Utils.openLink(anchorElem.href);
    };

    // Install a function on the document that calles 'saves as' dialog
    // for optimized image.
    this.document.saveLink = function(anchorElem) {
      PAGESPEED.Utils.saveLink(anchorElem.href);
    };

    // Display the welcome page.
    this.welcomePageTag.replace(
        {'dependencies': mismatchedDependencies}, this.panelNode);
  },

  /** @override */
  show: function(state) {
    if (this.showToolbarButtons) {
      this.showToolbarButtons('fbPageSpeedButtons', true);
    }
  },

  /** @override */
  hide: function() {
    if (this.showToolbarButtons) {
      this.showToolbarButtons('fbPageSpeedButtons', false);
    }
  },

  /**
   * Displays a small hover box showing the image when the user mouses over
   * any image url in this panel.
   *
   * @override
   */
  showInfoTip: function(infoTip, target, x, y) {
    var imageUrl = target.href;

    // Only work on links.
    if (target.tagName.toUpperCase() !== 'A' || !imageUrl) {
      return false;
    }

    // Make sure this is only called once per hover.
    if (imageUrl === this.infoTipURL) {
      return true;
    }

    // Only work on links to images.
    var types = PAGESPEED.Utils.getResourceTypes(imageUrl);
    for (var i = 0, length = types.length; i < length; ++i) {
      var type = types[i];
      if (type === 'image' || type === 'cssimage' || type === 'favicon') {
        this.infoTipURL = imageUrl;
        return Firebug.InfoTip.populateImageInfoTip(infoTip, imageUrl);
      }
    }

    return false;
  },

  /**
   * Get a list of items for the 'options' menu in the upper right corner of
   * the firebug pane.
   *
   * @override
   *
   * @return {Array} List of objects whose properties are used by firebug to
   *    build the options menu.
   */
  getOptionsMenuItems: function() {
    var menuOptions = [];

    /**
     * Given a menu item name and a function, add a menu item which calls that
     * function when selected.
     *
     * @param {string} label The text of the menu item.
     * @param {Function} onSelectMenuItem Called when the menu item is selected.
     * @param {boolean} checked Whether the menu item should be checked.
     * @param {boolean?} opt_disabled Whether the menu item should be disabled.
     */
    var addMenuOption = function(
        label, onSelectMenuItem, checked, opt_disabled) {
      var menuItemObj = {
        label: label,
        nol10n: true,  // Use the label as-is, rather than looking in a
                       // properties file.
        command: onSelectMenuItem,
        type: 'checkbox',
        checked: checked,
        disabled: Boolean(opt_disabled)
      };
      menuOptions.push(menuItemObj);
    };

    var jsProfileEnabledPref = 'extensions.PageSpeed.js_coverage.enable';
    var autoRunEnabledPref = 'extensions.PageSpeed.autorun';
    var showAllUserAgentsPref = 'extensions.PageSpeed.show_all_user_agents';

    /**
     * @param {string} prefName The name of a boolean preference.
     * @return {Function} A function that will toggle the value of that
     *     preference.
     */
    var buildToggleBoolPrefFn = function(prefName) {
      return function() {
        var oldValue = PAGESPEED.Utils.getBoolPref(prefName);
        PAGESPEED.Utils.setBoolPref(prefName, !oldValue);
      };
    };

    addMenuOption('Profile Deferrable JavaScript (slow)',
                  buildToggleBoolPrefFn(jsProfileEnabledPref),
                  PAGESPEED.Utils.getBoolPref(jsProfileEnabledPref));

    addMenuOption('Automatically Run at Onload',
                  buildToggleBoolPrefFn(autoRunEnabledPref),
                  PAGESPEED.Utils.getBoolPref(autoRunEnabledPref));

    addMenuOption('Show All User Agents',
                  buildToggleBoolPrefFn(showAllUserAgentsPref),
                  PAGESPEED.Utils.getBoolPref(showAllUserAgentsPref));

    menuOptions.push('-');

    // Add an unselectable menu item that serves as the heading for
    // the location of output files.
    addMenuOption('Save Optimized Files To:', function() {}, false, true);

    // The path and an nsILocalFile will be added to each object below.
    var outputDirOptions = {
      Temp: {mozKey: 'TmpD'},
      Home: {mozKey: 'Home'},
      Desktop: {mozKey: 'Desk'}
    };

    /**
     * Build a function that will set a preference to a file.
     * @param {nsIFile} file The file whose path will be set.
     */
    var buildSetFilePrefFn = function(file) {
      return function() {
        PAGESPEED.Utils.setOutputDir(file);
      };
    }

    // If the directory where optimized output will be written is unset, then
    // set to the default value.
    PAGESPEED.Utils.setDefaultOutputDir();

    var currentOutputDir = PAGESPEED.Utils.getOutputDir();
    var currentOutputPath = null;
    if (currentOutputDir)
      currentOutputPath = PAGESPEED.Utils.getPathForFile(currentOutputDir);

    // Set to true when the current output directory is added to the menu.
    // If it is not, than we will add an extra menu item to include it.
    var currentOutputDirSeen = false;

    for (var dirName in outputDirOptions) {
      var outDir = outputDirOptions[dirName];

      var outDirFile = PAGESPEED.Utils.CCSV(
        '@mozilla.org/file/directory_service;1', 'nsIProperties')
        .get(outDir.mozKey, Components.interfaces.nsIFile);

      // If the dir can not be accessed, than do not list it in the menu.
      if (!outDirFile) continue;

      outDir.file = outDirFile;
      outDir.path = PAGESPEED.Utils.getPathForFile(outDirFile);

      var menuText = [' ', dirName,
                      ' (', outDir.path, ')'].join('');

      var isSelected = (currentOutputPath === outDir.path);
      if (isSelected)
        currentOutputDirSeen = true;

      addMenuOption(menuText,
                    buildSetFilePrefFn(outDir.file),
                    isSelected
                    );
    }

    // If the value set in the preference has not been put in the menu
    // yet, add it.
    if (currentOutputPath && !currentOutputDirSeen) {
      addMenuOption([' Custom Setting: ', currentOutputPath].join(''),
                    function() {},
                    true
                    );
    }

    // Add a menu option to set a custom path.
    addMenuOption(
        ' Choose a Custom Path',
        function() {
          var fp = PAGESPEED.Utils.CCIN(
              '@mozilla.org/filepicker;1', 'nsIFilePicker');
          fp.init(window, 'Select a directory to store optomized results',
                  Components.interfaces.nsIFilePicker.modeGetFolder);

          // Set the start directory to the user's desktop dir, if it was
          // found above.
          var desktopDir = outputDirOptions['Desktop']['file'];
          if (desktopDir) {
            fp.displayDirectory = desktopDir;
          }

          if (fp.show() != Components.interfaces.nsIFilePicker.returnOK) {
            // User canceled.  Don't change the pref.
            return;
          }

          PAGESPEED.Utils.setOutputDir(fp.file);
        },
        false
        );

    menuOptions.push('-');

    var uac = PAGESPEED.UserAgentController;

    /**
     * Build a zero argument function which switches the user agent settings
     * to mimic a specific browser.
     * @param {string} userAgentName Name of the browser to set user agent
     *    data to.
     * @return {Function} A zero-argument function which sets the user agent to
     *    a specific browser.
     */
    var buildOnSelectUserAgentFn = function(userAgentName) {
      return function() {
        uac.setUserAgentByName(userAgentName);
      };
    };

    // Add an unselectable menu item that serves as the heading for
    // user agent options.
    addMenuOption('Set User Agent to:', function() {}, false, true);

    // Add an option to select the default user agent.
    addMenuOption('  Default Value',
                  function() {uac.resetUserAgent();},
                  uac.isDefaultUserAgent());

    // To cut down on menu clutter, only show user agents of recent,
    // popular browsers unless the user checks this option.
    var showAllUseragents = PAGESPEED.Utils.getBoolPref(showAllUserAgentsPref);

    // Add an option for each registered user agent.
    var userAgentNames = uac.getUserAgentNames(showAllUseragents);
    for (var i = 0, len = userAgentNames.length; i < len; ++i) {
      addMenuOption('  ' + userAgentNames[i],
                    buildOnSelectUserAgentFn(userAgentNames[i]),
                    uac.isCurrentUserAgent(userAgentNames[i]));
    }

    return menuOptions;
  },

  /**
   * Handles the onClick event for rules in the panel, showing the details if
   * the rule has details.
   */
  showDetails: function(event) {
    this.onClick(event, this.toggleDetailsRow);
  },

  /**
   * Handles the onClick event for resources in the panel, showing the headers
   * if available.
   */
  showHeaders: function(event) {
    this.onClick(event, this.toggleHeadersRow);
  },

  /**
   * Handles a click on a row in the resources or performance tab by calling
   * the given toggleMethod with a reference to the clicked row.
   */
  onClick: function(event, toggleRowMethod) {
    if (FBL.isLeftClick(event)) {
      var row = FBL.getAncestorByClass(event.target, 'netRow');
      if (row) {
        if (FBL.hasClass(row, 'hasHeaders')) {
          FBL.toggleClass(row, 'opened');

          if (FBL.hasClass(row, 'opened')) {
            toggleRowMethod.call(this, row);
          } else {
            row.parentNode.removeChild(row.nextSibling);
          }
        }
        FBL.cancelEvent(event);
      }
    }
  },

  /**
   * Handles the onClick event for the "Analyze Performance" button.
   */
  analyzePerformance: function(event) {
    Firebug.PageSpeedModule.analyzePerformance();
  },

  /**
   * Handles links correctly (either new tab or new window).
   * @param {Event} event The event to handle.
   */
  openLink: function(event) {
    // Don't do the normal action for this click event (i.e. following the link
    // in the usual manner).
    event.preventDefault();
    // Don't propagate this event to other elements (in the case of rule
    // documentation links, doing so would trigger toggleDetailsRow, which we
    // don't want in this case).
    event.stopPropagation();
    // Open the link in a new tab/window.
    PAGESPEED.Utils.openLink(event.target);
  },

  /**
   * Toggles the display of the details for the given row.
   */
  toggleDetailsRow: function(row) {
    var detailsRow = this.detailsTag.insertRows({}, row)[0];
    detailsRow.firstChild.firstChild.innerHTML = row.repObject.details;
  },

  /**
   * Toggles the display of the headers for a given resource.
   */
  toggleHeadersRow: function(row) {
    var headersRow = this.headersTag.insertRows({}, row)[0];

    // Create an empty object that we'll populate with the fields that
    // the net panel template expects.
    var url = row.repObject.href;
    var netPanelFile = {};

    // Just guess that this is a GET request, since it doesn't really
    // matter in this context.
    netPanelFile.method = 'GET';

    // These fields are required in order to populate the 'Response'
    // tab.
    netPanelFile.href = url;
    netPanelFile.loaded = true;
    netPanelFile.category = getNetPanelCategory(url);
    if (netPanelFile.category == 'txt') {
      netPanelFile.responseText = PAGESPEED.Utils.getResourceContent(url);
    } else {
      // Disable fetching of the response contents for non-text
      // responses.
      netPanelFile.responseText = '';
    }

    // Fill in the request and response headers. Required for the
    // 'Headers' tab.
    addHeadersToNetPanelEntry(
        netPanelFile,
        PAGESPEED.Utils.NETPANEL_RESPONSE_HEADER,
        PAGESPEED.Utils.getResponseHeaders(url));
    addHeadersToNetPanelEntry(
        netPanelFile,
        PAGESPEED.Utils.NETPANEL_REQUEST_HEADER,
        PAGESPEED.Utils.getRequestHeaders(url));

    var template = Firebug.NetMonitor.NetInfoBody;
    var netInfo = template.tag.replace({'file': netPanelFile},
                                       headersRow.firstChild);
    template.selectTabByName(netInfo, 'Headers');
  },

  /**
   * Updates the progressbar to the given percent.
   * @param {number} nPercentComplete number in range [0..100].
   * @param {string} opt_sText optional descriptive text.
   * @param {string} opt_sSubText optional descriptive subtext.
   */
  setProgress: function(nPercentComplete, opt_sText, opt_sSubText) {
    if (opt_sText) this.progressText = opt_sText;
    this.progressTag.replace({'width': nPercentComplete * 2 + 'px',
                              'text': this.progressText,
                              'subtext': opt_sSubText || ''},
                             this.panelNode);
  }
});

/**
 * Method that determines the appropriate net panel category for a
 * resource. Used by toggleHeadersRow which uses the NetInfoBody
 * template to populate the resources tab.
 * @param {string} url The URL to determine the net panel category
 *     for.
 * @return {string} The net panel category for the given resource.
 */
function getNetPanelCategory(url) {
  var types = PAGESPEED.Utils.getResourceTypes(url);
  for (var i = 0, len = types.length; i < len; ++i) {
    switch (types[i]) {
      case 'image':
      case 'cssimage':
      case 'favicon':
        return 'image';
      case 'js':
      case 'css':
      case 'doc':
      case 'iframe':
      case 'other':
        return 'txt';
    }
  }

  // Not all other files are type 'bin', but we don't want to display
  // a 'resource' tab for other resources, and netpanel will hide the
  // 'response' tab for any resources that are marked as type 'bin'.
  return 'bin';
}

/**
 * Used by toggleHeadersRow which uses the NetInfoBody template to
 * populate the resources tab.
 * @param {Object} netPanelFile the netpanel file entry.
 * @param {string} propName The property name to update.
 * @param {Object} headers The map of HTTP headers.
 * @return {Object} the old value for the given property name.
 */
function addHeadersToNetPanelEntry(netPanelFile, propName, headers) {
  var oldValue = netPanelFile[propName];
  netPanelFile[propName] = [];
  for (var header in headers) {
    netPanelFile[propName].push({'name': header, 'value': headers[header]});
  }
  return oldValue;
}

Firebug.registerPanel(PageSpeedPanel);

}});  // FBL.ns
