/*
 * Copyright 2012 AOL Inc. All rights reserved.
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
/*jshint
  browser: true, jquery: true, plusplus: false, curly: true, eqeqeq: true, immed: true, latedef: true, newcap: true, nonew: true, undef: true, strict: true, trailing: true, globalstrict: true
*/
/*global initialDisplayLength:true*/
"use strict";


//////////////////////////////////////////////////////////////////////////////////
// Utility Functions
//////////////////////////////////////////////////////////////////////////////////
function twoDigitString(value) {
  return (value < 10) ? ("0" + value) : value.toString();
}

function dateString(seconds, sep) {
  var d = new Date(seconds*1000);
  return d.getFullYear() +
         "/" +
         twoDigitString(d.getMonth()+ 1) +
         "/" +
         twoDigitString(d.getDate()) +
         sep +
         twoDigitString(d.getHours()) +
         ":" +
         twoDigitString(d.getMinutes()) +
         ":" +
         twoDigitString(d.getSeconds());
}

function ipString(ip) {
  return (ip>>24 & 0xff) + '.' + (ip>>16 & 0xff) + '.' + (ip>>8 & 0xff) + '.' + (ip & 0xff);
}

function parseUrlParams() {
  var urlParams = {};
  var e,
      a = /\+/g,  // Regex for replacing addition symbol with a space
      r = /([^&=]+)=?([^&]*)/g,
      d = function (s) { return decodeURIComponent(s.replace(a, " ")); },
      q = window.location.search.substring(1);

  while ((e = r.exec(q))) {
     urlParams[d(e[1])] = d(e[2]);
  }

  return urlParams;

}

// From http://stackoverflow.com/a/2901298
function numberWithCommas(x) {
  return x.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",");
}

// From http://datatables.net/plug-ins/api#fnLengthChange
$.fn.dataTableExt.oApi.fnLengthChange = function ( oSettings, iDisplay )
{
    oSettings._iDisplayLength = iDisplay;
    oSettings.oApi._fnCalculateEnd( oSettings );

    /* If we have space to show extra rows (backing up from the end point - then do so */
    if ( oSettings._iDisplayEnd === oSettings.aiDisplay.length )
    {
        oSettings._iDisplayStart = oSettings._iDisplayEnd - oSettings._iDisplayLength;
        if ( oSettings._iDisplayStart < 0 )
        {
            oSettings._iDisplayStart = 0;
        }
    }

    if ( oSettings._iDisplayLength === -1 )
    {
        oSettings._iDisplayStart = 0;
    }

    oSettings.oApi._fnDraw( oSettings );

    if ( oSettings.aanFeatures.l )
    {
        $('select', oSettings.aanFeatures.l).val( iDisplay );
    }
};

//From http://datatables.net/plug-ins/sorting
jQuery.extend( jQuery.fn.dataTableExt.oSort, {
    "formatted-num-pre": function ( a ) {
        a = (a==="-") ? 0 : a.replace( /[^\d\-\.]/g, "" );
        return parseFloat( a );
    },

    "formatted-num-asc": function ( a, b ) {
        return a - b;
    },

    "formatted-num-desc": function ( a, b ) {
        return b - a;
    }
} );

// From http://stackoverflow.com/a/8999941
(function ($, undefined) {
    $.fn.getCursorPosition = function() {
        var el = $(this).get(0);
        var pos = 0;
        if('selectionStart' in el) {
            pos = el.selectionStart;
        } else if('selection' in document) {
            el.focus();
            var Sel = document.selection.createRange();
            var SelLength = document.selection.createRange().text.length;
            Sel.moveStart('character', -el.value.length);
            pos = Sel.text.length - SelLength;
        }
        return pos;
    };
}(jQuery));

(function ($, undefined) {
    $.fn.getCurrentWord = function() {
      var val = $(this).val();
      var start, end;
      for (start = $(this).getCursorPosition(); start > 0; start--) {
        if (val[start-1] === " ") {
          break;
        }
      }

      for (end = $(this).getCursorPosition(); end < val.length; end++) {
        if (val[end] === " ") {
          break;
        }
      }

      return val.substring(start, end);
    };
}(jQuery));


function splitExpression(input ) {
  input = input.replace(/ /g, "");
  var output = [];
  var cur = "";

  for (var i = 0; i < input.length; i++) {
      if (/[)(]/.test(input[i])) {
        if (cur !== "") {
          output.push(cur);
        }
        output.push(input[i]);
        cur = "";
      } else if (cur === "") {
        cur += input[i];
      } else if (/[!&|=]/.test(cur)) {
        if (/[&|=]/.test(input[i])) {
          cur += input[i];
        } else {
          output.push(cur);
          cur = input[i];
        }
      } else if (/[!&|=]/.test(input[i])) {
        output.push(cur);
        cur = input[i];
      } else {
        cur += input[i];
      }
  }
  if (cur !== "") {
    output.push(cur);
  }
  return output;
}

function updateHealth(health)
{
  if (health === undefined || health.status === undefined) {
    return $("#esstatus").hide();
  }

  $("#esstatus").show();
  $("#esstatus").css("background", health.status);
  $("#esstatus").qtip({content:
     "Elasticsearch:<br>" +
     " Status: " + health.status + "<br>" +
     " Nodes: " + health.number_of_data_nodes + "<br>" +
     " Shards: " + health.active_shards + "<br>" +
     " Relocating Shards: " + health.relocating_shards + "<br>" +
     " Unassigned Shards: " + health.unassigned_shards + "<br>",
     position: {
       my: 'top right',
       at: 'bottom left'
     },
     style: {
       classes: 'qtip-moloch'
     }});
}

function startBlink() {
  $(".blink").animate({opacity:0},500,"linear")
             .animate({opacity:1},500,"linear",startBlink);
}

function stopBlink() {
  $(".blink").stop(true,true);
}

function updateParam(param, n, v) {
  for (var i = 0; i < param.length; i++) {
    if (param[i].name === n) {
      param[i].value = v;
      return;
    }
  }
  param.push({name: n, value: v});
}

//////////////////////////////////////////////////////////////////////////////////
// layout Functions
//////////////////////////////////////////////////////////////////////////////////
$(document).ready(function() {
  $('.tooltip').qtip();

  $(".expressionsLink").click(function (e) {
    var data;
    if (typeof sessionsTable !== 'undefined') {
      data = sessionsTable.fnSettings().oApi._fnAjaxParameters(sessionsTable.fnSettings());
    } else {
      data = [];
    }

    var params = buildParams();
    params = $.merge(data, params);

    var url = $(e.target).attr("href") + "?" + $.param(params);

    window.location = url;
    return false;
  });

  $('#date').change(function() {
    var hours = parseInt($("#date").val(), 10);
    if (hours === -2) {
      $("#customDate").show();
    } else {
      if (hours !== -1) {
        $("#startDate").val(dateString(new Date()/1000 - 60*60*hours, ' '));
        $("#stopDate").val(dateString(new Date()/1000, ' '));
      } else {
        $("#startDate").val(dateString(0, ' '));
        $("#stopDate").val(dateString(new Date()/1000, ' '));
      }
      $("#customDate").hide();
    }

    $('#searchForm').submit();
    return false;
  });

  $("#export").click(function (e) {
    if (typeof sessionsTable !== 'undefined') {
      $("#export-opened-div").show();
      $("#export-opened-cnt").text(numberWithCommas($("tr.opened").length));
    } else {
      $("#export-opened-div").hide();
    }

    if ($("#exportForm").data("moloch-visible") === -1) {
      $("#export-visible-div").hide();
      $("#export-all").prop("checked", true);
    } else {
      $("#export-visible-div").show();
      $("#export-visible").prop("checked", true);
      $("#export-visible-cnt").text(numberWithCommas($("#exportForm").data("moloch-visible")));
    }

    $("#export-all-cnt").text(numberWithCommas($("#exportForm").data("moloch-all")));

    $('<div/>').qtip({
      id: "exportDialog",
      content: {
        text: $('#exportForm'),
        title: {
          text: "Export PCAP",
          button: true
        }
      },
      position: {
        my: 'center', // ...at the center of the viewport
        at: 'top center',
        target: $(window),
        adjust: {
          y: 100
        }
      },
      show: {
        event: 'click', // Show it on click...
        ready: true, // Show it immediately on page load.. force them to login!
        modal: {
          on: true,

          // Don't let users exit the modal in any way
          blur: false, escape: false
        }
      },
      hide: false,
      style: {
        classes: 'qtip-light qtip-rounded',
        tip: false
      },
      events: {
        render: function(event, api) {
          // Capture the form submission
          setTimeout(function () {
            $("#exportfilename").select().focus();
          }, 100);
          $('form', this).bind('submit', function(event) {
            $(this).parents('.qtip').qtip("hide");
            // Prevent normal form submission
            event.preventDefault();

            var type = $('input[name=export-type]:checked').val();
            var data;

            if (typeof sessionsTable !== 'undefined') {
              data = sessionsTable.fnSettings().oApi._fnAjaxParameters(sessionsTable.fnSettings());
              if (type === "all") {
                updateParam(data, "iDisplayStart", 0);
                updateParam(data, "iDisplayLength", sessionsTable.fnSettings().fnRecordsDisplay());
              } else if (type === "opened") {
                var ids = [];
                $("tr.opened").each(function(n, nTr) {
                  var rowData = sessionsTable.fnGetData(nTr);
                  ids.push(rowData.id);
                });
                data = [{name: "ids", value: ids}];
              }
            } else {
              data = [];
              if (type === "visible") {
                updateParam(data, "iDisplayLength", $("#exportForm").data("moloch-visible"));
              } else  {
                updateParam(data, "iDisplayLength", $("#exportForm").data("moloch-all"));
              }
            }

            var url;
            if (type === "opened") {
              url = "sessionIds.pcap/" + $("#exportfilename").val() + "?" + $.param(data);
            } else {
              var params = buildParams();
              params = $.merge(data, params);

              url = "sessions.pcap/" + $("#exportfilename").val() + "?" + $.param(params);
            }

            window.location = url;
          });
        }
      }
    });
    return false;
  });

  $('#startDate,#stopDate').keypress(function (e) {
    if ((e.keyCode ? e.keyCode : e.which) === 13) {
      $('#searchForm').submit();
      return false;
    }
  });
});

//2013-02-27 18:14:41 UTC
var utcParser = d3.time.format.utc("%Y-%m-%d %X UTC").parse;
function handleUrlParams() {
  var urlParams = parseUrlParams();

  if (urlParams.date) {
    $("#date").val(urlParams.date);
    if (urlParams.date !== -1) {
      $("#startDate").val(dateString(new Date()/1000 - 60*60*urlParams.date, ' '));
      $("#stopDate").val(dateString(new Date()/1000, ' '));
    } else {
      $("#startDate").val(dateString(0, ' '));
      $("#stopDate").val(dateString(new Date()/1000, ' '));
    }
  } else {
    $("#date").val("");
  }

  if (urlParams.expression) {
    $("#expression").val(urlParams.expression);
  } else {
    $("dexpression").val("");
  }

  initialDisplayLength = urlParams.iDisplayLength || 100;
  $("#graphSize").val(String(initialDisplayLength));
  $("#sessions_length").val(String(initialDisplayLength));

  if (urlParams.startTime && urlParams.stopTime) {

    var st;
    if (! /^[0-9]+$/.test(urlParams.startTime)) {
      st = Date.parse(urlParams.startTime.replace("+", " "))/1000;
      if (isNaN(st) || st === null) {
        st = utcParser(urlParams.startTime).getTime()/1000;
      }
      urlParams.startTime = st;
    }

    if (! /^[0-9]+$/.test(urlParams.stopTime)) {
      st = Date.parse(urlParams.stopTime.replace("+", " "))/1000;
      if (isNaN(st) || st === null) {
        st = utcParser(urlParams.stopTime).getTime()/1000;
      }
      urlParams.stopTime = st;
    }

    $("#startDate").val(dateString(urlParams.startTime, ' '));
    $("#stopDate").val(dateString(urlParams.stopTime, ' '));
    $("#date").val("-2");
  } else if (urlParams.centerTime && urlParams.timeWindow) {
    var ct;
    if (! /^[0-9]+$/.test(urlParams.centerTime)) {
      ct = Date.parse(urlParams.centerTime.replace("+", " "))/1000;
      if (isNaN(ct) || ct === null) {
        ct = utcParser(urlParams.centerTime).getTime()/1000;
      }
    } else {
      ct = urlParams.centerTime;
    }

    urlParams.startTime = ct - parseInt(urlParams.timeWindow,10)*60;
    urlParams.stopTime = ct + parseInt(urlParams.timeWindow,10)*60;

    $("#startDate").val(dateString(urlParams.startTime, ' '));
    $("#stopDate").val(dateString(urlParams.stopTime, ' '));
    $("#date").val("-2");
  }

  if (urlParams.useDir=== "0" && urlParams.usePort === "0") {
    $("#graphType").val("useDir=0&usePort=0");
  } else if (urlParams.useDir=== "0" && urlParams.usePort === "1") {
    $("#graphType").val("useDir=0&usePort=1");
  } else if (urlParams.useDir=== "1" && urlParams.usePort === "0") {
    $("#graphType").val("useDir=1&usePort=0");
  } else if (urlParams.useDir=== "1" && urlParams.usePort === "1") {
    $("#graphType").val("useDir=1&usePort=1");
  }

  return urlParams;
}

function addExpression (expression, op) {
  var val = $("#expression").val();
  if (val === "") {
    $("#expression").val(expression);
  } else {
    $("#expression").val(val + (op || " && ") + expression);
  }
  return false;
}

$(document).ready(function() {
  if ($("#expression").length && $("#expression").autocomplete) {
    $("#expression").autocomplete("", {
      useDelimiter: true,
      delimiterChar: ' ',
      minChars: 0,
      maxItemsToShow: 100
    });

    $("#expression").data('autocompleter').fetchRemoteData = function(filter,callback) {
      var cp = $("#expression").getCursorPosition();
      var input = $("#expression").val();

      var spaceCP = (cp > 0 && cp === input.length && input[cp-1] === " ");

      for (var end = cp; end < input.length; end++) {
        if (input[end] === " ") {
          break;
        }
      }

      input = input.substr(0, end);
      var tokens = splitExpression(input);
      if (spaceCP) {
        tokens.push(" ");
      }

      if (tokens.length <= 1) {
        return callback(molochFieldsExp);
      }

      // Operator
      var token = tokens[tokens.length-2];
      var field = molochFields[token];
      if (field !== undefined) {
        if (field.type === "integer")  {
          return callback(["!=", "==" , ">=", "<=", "<", ">"]);
        } else {
          return callback(["!=", "=="]);
        }
      }

      // Values
      token = tokens[tokens.length-3];
      field = molochFields[token];

      if (!field) {
        if (/^[!<=>]/.test(token)) {
          return callback(["&&", "||", ")"]);
        }
        return callback(molochFieldsExp);
      }

      if (/^(country)/.test(token)) {
        return callback(Object.keys($(document).data("countries")));
      }


      if (/^(tags|http.hasheader)/.test(token)) {
        if (filter.length < 1) {
          return callback([]);
        }
        $.ajax( {
            "dataType": 'html',
            "type": "GET",
            "url": 'uniqueValue.json?type=' + token + '&filter=' + filter,
            "success": function(data) {
              return callback(JSON.parse(data));
            }
        } );
        return;
      }

      return callback([]);
    };
  }
});

function addSessionIPStr (ip) {
  return addExpression("ip == " + ip);
}

function addSessionIP (ip) {
  return addSessionIPStr(ipString(ip));
}

function setSessionStartTime (t) {
  $("#startDate").val(dateString(t, ' '));
  $("#date").val("-2").change();
  return false;
}

function setSessionStopTime (t) {
  $("#stopDate").val(dateString(t, ' '));
  $("#date").val("-2").change();
  return false;
}

function buildParams() {
  var params = [];

  if ($("#date").length) {
    if ($("#date").val() === "-2") {
      /* Date madness because of firefox on windows */
      var d = new Date($("#startDate").val());
      if (d < 0) {d.setFullYear(d.getFullYear() + 100);}
      params.push({name:'startTime', value:d/1000});

      d = new Date($("#stopDate").val());
      if (d < 0) {d.setFullYear(d.getFullYear() + 100);}
      params.push({name:'stopTime', value:d/1000});
    } else if ($("#date").val()) {
      params.push({name:'date', value:$("#date").val()});
    }
  }

  if ($("#expression").length) {
    if ($("#expression").val()) {
      params.push({name:'expression', value:$("#expression").val()});
    }
  }

  if (typeof sessionsTable === 'undefined') {
    params.push({name: "iDisplayLength", value: initialDisplayLength});
  }

  return params;
}

//////////////////////////////////////////////////////////////////////////////////
// Graph Functions
//////////////////////////////////////////////////////////////////////////////////

function updateGraph(allGraphData, graphId) {
  graphId = graphId || "#sessionGraph";

  if (!allGraphData) {
    return;
  }
  $(graphId).data("molochGraphData", allGraphData);
  drawGraph($('#sessionGraphSelect').val(), graphId);
}

function drawGraph(graphName, graphId) {
  graphId = graphId || "#sessionGraph";

  var allGraphData = $(graphId).data("molochGraphData");
  var graphData = allGraphData[graphName];
  if (!graphData) {
    return;
  }
  var interval = allGraphData.interval * 1000;

  var color = "#000000";
  var plot = $.plot(
  $(graphId), [{
    data: graphData
  }], {
    series: {
      lines: {
        show: false,
        fill: true
      },
      bars: {
        show: true,
        fill: 1,
        barWidth: interval / 1.7
      },
      points: {
        show: false
      },
      color: color,
      shadowSize: 0
    },
    xaxis: {
      mode: "time",
      label: "Datetime",
      color: "#000",
      tickFormatter: function(v, axis) {
        return dateString(v/1000, "<br>");
      },
      min: allGraphData.xmin || null,
      max: allGraphData.xmax || null
    },
    yaxis: {
      min: 0,
      color: "#000",
      tickFormatter: function(v, axis) {
        return numberWithCommas(v);
      }
    },
    selection: {
      mode: "x",
      color: '#000'
    },
    zoom : {
      interactive: false,
      trigger: "dblclick",
      amount: 2
    },
    pan : {
      interactive: false,
      cursor: "move",
      frameRate: 20
    },
    legend: {
      position: "nw"
    },
    grid: {
      backgroundColor: '#fff',
      borderWidth: 0,
      borderColor: '#000',
      color: "#ddd",
      hoverable: true,
      clickable: true
    }
  });

  // code from flot - add zoom out button
  $('<div class="button" style="right:20px;top:5px">zoom out</div>').appendTo($(graphId)).click(function (e) {
      e.preventDefault();
      plot.zoomOut();
  });

  function addArrow(dir, right, top, offset) {
      $('<img class="button" src="flot-0.7/examples/arrow-' + dir + '.gif" style="right:' + right + 'px;top:' + top + 'px">').appendTo($(graphId)).click(function (e) {
        e.preventDefault();
        plot.pan(offset);
      });
  }

  addArrow('left', 115, 5, { left: -100 });
  addArrow('right', 85, 5, { left: 100 });
}

    // Code from Kibana
function showTooltip(x, y, contents) {
  $('<div id="tooltip">' + contents + '</div>').css({
    position: 'absolute',
    display: 'none',
    top: y - 20,
    left: x - 100,
    color: '#eee',
    padding: '3px',
    'font-size': '8pt',
    'background-color': '#000',
    border: '1px solid #000',
    'font-family': '"Verdana", Geneva, sans-serif'
  }).appendTo("body").fadeIn(200);
}

function setupGraph(graphId) {
  graphId = graphId || "#sessionGraph";

  // Pieces from Kibana
  var previousPoint = null;
  $(graphId).bind("plothover", function (event, pos, item) {
    $("#x").text(pos.x.toFixed(2));
    $("#y").text(pos.y.toFixed(2));

    if (item) {
      if (previousPoint !== item.dataIndex) {
        previousPoint = item.dataIndex;

        $("#tooltip").remove();
        var x = item.datapoint[0].toFixed(0),
            y = Math.round(item.datapoint[1]*100)/100,
            d = dateString(x/1000, " ");

        showTooltip(item.pageX, item.pageY, numberWithCommas(y) + " at " + d.substr(0, d.length));
      }
    } else {
      $("#tooltip").remove();
      previousPoint = null;
    }
  });

  $(graphId).bind("plotselected", function (event, ranges) {
    $("#startDate").val(dateString(ranges.xaxis.from/1000, ' '));
    $("#stopDate").val(dateString(ranges.xaxis.to/1000, ' '));
    $("#date").val("-2").change();
  });

  $(graphId).bind('plotpan plotzoom', function (event, plot) {
    var axes = plot.getAxes();
    $("#startDate").val(dateString((axes.xaxis.min/1000)-1, ' '));
    $("#stopDate").val(dateString((axes.xaxis.max/1000)+1, ' '));
    $("#date").val("-2").change();
  });

  $('#sessionGraphSelect').change(function() {
    $(".sessionGraph").each(function(index, item) {
      drawGraph($('#sessionGraphSelect').val(), "#" + item.id);
    });
    return false;
  });
}

//////////////////////////////////////////////////////////////////////////////////
// Map Functions
//////////////////////////////////////////////////////////////////////////////////
function updateMap(data, mapId) {
  mapId = mapId || '#world-map';

  if (!data) {
    return;
  }

  var map = $(mapId).children('.jvectormap-container').data('mapObject');
  map.series.regions[0].clear();
  delete map.series.regions[0].params.min;
  delete map.series.regions[0].params.max;
  map.series.regions[0].setValues(data);
}

function setupMap(mapId) {
  mapId = mapId || '#world-map';

  $(mapId).vectorMap({
    map: 'world_en',
    backgroundColor: '#445b9a',
    hoverColor: 'black',
    hoverOpacity: 0.7,
    onRegionLabelShow: function(e, el, code){
      var map = $(mapId).children('.jvectormap-container').data('mapObject');
      el.html(el.html() + ' - ' + numberWithCommas(map.series.regions[0].values[code] || 0));
    },
    onRegionClick: function(e, code){
      addExpression("country == " + code);
    },
    series: {
      regions: [{
        scale: ['#bae4b3', '#006d2c'],
        normalizeFunction: 'polynomial'
      }]
    }
  });

  if ($(document).data("countries") === undefined) {
    var map = $(mapId).children('.jvectormap-container').data('mapObject');
    $(document).data("countries", map.regions);
  }

  $(mapId).children().hoverIntent (
    function() {
      $(this).parent().parent().css({
        'z-index': 3
      });
      var top = Math.max($(this).offset().top - $(window).scrollTop(), 0);
      $(this).parent().css({
        position: "fixed",
        right: 0,
        top: Math.min(top, $(window).height() * 0.25),
        width: $(window).width()*0.75,
        height: $(window).height()*0.75
      });
      $(this).parent().resize();
    },
    function(e) {
      if (e.relatedTarget && e.relatedTarget.className === "jvectormap-label") {
        return;
      }

      $(this).parent().parent().css({
        'z-index': 2
      });
      $(this).parent().css({
        position: "relative",
        right: 0,
        top: 0,
        width: "250px",
        height: "150px"
      });
      $(this).parent().resize();
    }
  );
}

/*
* jQuery.ajaxQueue - A queue for ajax requests
*
* (c) 2011 Corey Frang
* Dual licensed under the MIT and GPL licenses.
*
* Requires jQuery 1.5+
*/
(function($) {

$.ajaxQueue = function(theQueue, ajaxOpts ) {
    var jqXHR,
        dfd = $.Deferred(),
        promise = dfd.promise();

    // queue our ajax request
    theQueue.queue( doRequest );

    // add the abort method
    promise.abort = function( statusText ) {

        // proxy abort to the jqXHR if it is active
        if ( jqXHR ) {
            return jqXHR.abort( statusText );
        }

        // if there wasn't already a jqXHR we need to remove from queue
        var queue = theQueue.queue(),
            index = $.inArray( doRequest, queue );

        if ( index > -1 ) {
            queue.splice( index, 1 );
        }

        // and then reject the deferred
        dfd.rejectWith( ajaxOpts.context || ajaxOpts, [ promise, statusText, "" ] );
        return promise;
    };

    // run the actual query
    function doRequest( next ) {
        jqXHR = $.ajax( ajaxOpts )
            .done( dfd.resolve )
            .fail( dfd.reject )
            .then( next, next );
    }

    return promise;
};

})(jQuery);

/*!
 * hoverIntent r7 // 2013.03.11 // jQuery 1.9.1+
 * http://cherne.net/brian/resources/jquery.hoverIntent.html
 *
 * You may use hoverIntent under the terms of the MIT license.
 * Copyright 2007, 2013 Brian Cherne
 */
(function(e){e.fn.hoverIntent=function(t,n,r){var i={interval:100,sensitivity:7,timeout:0};if(typeof t==="object"){i=e.extend(i,t)}else if(e.isFunction(n)){i=e.extend(i,{over:t,out:n,selector:r})}else{i=e.extend(i,{over:t,out:t,selector:n})}var s,o,u,a;var f=function(e){s=e.pageX;o=e.pageY};var l=function(t,n){n.hoverIntent_t=clearTimeout(n.hoverIntent_t);if(Math.abs(u-s)+Math.abs(a-o)<i.sensitivity){e(n).off("mousemove.hoverIntent",f);n.hoverIntent_s=1;return i.over.apply(n,[t])}else{u=s;a=o;n.hoverIntent_t=setTimeout(function(){l(t,n)},i.interval)}};var c=function(e,t){t.hoverIntent_t=clearTimeout(t.hoverIntent_t);t.hoverIntent_s=0;return i.out.apply(t,[e])};var h=function(t){var n=jQuery.extend({},t);var r=this;if(r.hoverIntent_t){r.hoverIntent_t=clearTimeout(r.hoverIntent_t)}if(t.type=="mouseenter"){u=n.pageX;a=n.pageY;e(r).on("mousemove.hoverIntent",f);if(r.hoverIntent_s!=1){r.hoverIntent_t=setTimeout(function(){l(n,r)},i.interval)}}else{e(r).off("mousemove.hoverIntent",f);if(r.hoverIntent_s==1){r.hoverIntent_t=setTimeout(function(){c(n,r)},i.timeout)}}};return this.on({"mouseenter.hoverIntent":h,"mouseleave.hoverIntent":h},i.selector)}})(jQuery);
