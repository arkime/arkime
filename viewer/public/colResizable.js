/**
           _ _____           _          _     _
          | |  __ \         (_)        | |   | |
  ___ ___ | | |__) |___  ___ _ ______ _| |__ | | ___
 / __/ _ \| |  _  // _ \/ __| |_  / _` | '_ \| |/ _ \
| (_| (_) | | | \ \  __/\__ \ |/ / (_| | |_) | |  __/
 \___\___/|_|_|  \_\___||___/_/___\__,_|_.__/|_|\___|

 v1.7 - jQuery plugin created by Alvaro Prieto Lauroba

 Licences: MIT & GPL
 Feel free to use or modify this plugin as far as my full name is kept
 */

(function($){

  var d = $(document); 		//window object
  var h = $("head");			//head object
  var drag = null;			//reference to the current grip that is being dragged
  var tables = {};			//object of the already processed tables (table.id as key)
  var	count = 0;				//internal count to create unique IDs when needed.

  //common strings for packing
  var ID = "id";
  var PX = "px";
  var SIGNATURE ="JColResizer";
  var FLEX = "JCLRFlex";

  //short-cuts
  var I = parseInt;
  var M = Math;
  var ie = navigator.userAgent.indexOf('Trident/4.0')>0;
  var S;
  var pad = ""
  try{S = sessionStorage;}catch(e){}	//Firefox crashes when executed as local file system


  //append required CSS rules
  h.append("<style type='text/css'>  .JColResizer{table-layout:fixed;} .JColResizer > tbody > tr > td, .JColResizer > tbody > tr > th{overflow:hidden}  .JPadding > tbody > tr > td, .JPadding > tbody > tr > th{padding-left:0!important; padding-right:0!important;} .JCLRgrips{ height:0px; position:relative;} .JCLRgrip{margin-left:-5px; position:absolute; z-index:5; } .JCLRgrip .JColResizer{position:absolute;background-color:red;filter:alpha(opacity=1);opacity:0;width:10px;height:100%;cursor: col-resize;top:0px} .JCLRLastGrip{position:absolute; width:1px; } .JCLRgripDrag{ border-left:1px dotted black;	} .JCLRFlex{width:auto!important;} .JCLRgrip.JCLRdisabledGrip .JColResizer{cursor:default; display:none;}</style>");


  /**
   * Function to allow column resizing for table objects. It is the starting point to apply the plugin.
   * @param {DOM node} tb - reference to the DOM table object to be enhanced
   * @param {Object} options	- some customization values
   */
  var init = function( tb, options){
    var t = $(tb);				    //the table object is wrapped
    t.opt = options;                //each table has its own options available at anytime
    t.mode = options.resizeMode;    //shortcuts
    t.dc = t.opt.disabledColumns;
    if(t.opt.removePadding) t.addClass("JPadding");
    if(t.opt.disable) return destroy(t);				//the user is asking to destroy a previously colResized table
    var	id = t.id = t.attr(ID) || SIGNATURE+count++;	//its id is obtained, if null new one is generated
    t.p = t.opt.postbackSafe; 							//short-cut to detect postback safe
    if(!t.is("table") || tables[id] && !t.opt.partialRefresh) return; 		//if the object is not a table or if it was already processed then it is ignored.
    if (t.opt.hoverCursor !== 'col-resize') h.append("<style type='text/css'>.JCLRgrip .JColResizer:hover{cursor:"+ t.opt.hoverCursor +"!important}</style>");  //if hoverCursor has been set, append the style
    t.addClass(SIGNATURE).attr(ID, id).before('<div class="JCLRgrips"/>');	//the grips container object is added. Signature class forces table rendering in fixed-layout mode to prevent column's min-width
    t.g = []; t.c = []; t.w = t.width(); t.gc = t.prev(); t.f=t.opt.fixed;	//t.c and t.g are arrays of columns and grips respectively
    if(options.marginLeft) t.gc.css("marginLeft", options.marginLeft);  	//if the table contains margins, it must be specified
    if(options.marginRight) t.gc.css("marginRight", options.marginRight);  	//since there is no (direct) way to obtain margin values in its original units (%, em, ...)
    t.cs = I(ie? tb.cellSpacing || tb.currentStyle.borderSpacing :t.css('border-spacing'))||2;	//table cellspacing (not even jQuery is fully cross-browser)
    t.b  = I(ie? tb.border || tb.currentStyle.borderLeftWidth :t.css('border-left-width'))||1;	//outer border width (again cross-browser issues)
    // if(!(tb.style.width || tb.width)) t.width(t.width()); //I am not an IE fan at all, but it is a pity that only IE has the currentStyle attribute working as expected. For this reason I can not check easily if the table has an explicit width or if it is rendered as "auto"
    tables[id] = t; 	//the table object is stored using its id as key
    createGrips(t);		//grips are created
  };


  /**
   * This function allows to remove any enhancements performed by this plugin on a previously processed table.
   * @param {jQuery ref} t - table object
   */
  var destroy = function(t){
    var id=t.attr(ID), t=tables[id];		//its table object is found
    if(!t||!t.is("table")) return;			//if none, then it wasn't processed
    t.removeClass(SIGNATURE+" "+FLEX).gc.remove();	//class and grips are removed
    delete tables[id];						//clean up data
  };


  /**
   * Function to create all the grips associated with the table given by parameters
   * @param {jQuery ref} t - table object
   */
  var createGrips = function(t){
    var th = t.find(">thead>tr:first>th,>thead>tr:first>td"); //table headers are obtained
    if(!th.length) th = t.find(">tbody>tr:first>th,>tr:first>th,>tbody>tr:first>td, >tr:first>td");	 //but headers can also be included in different ways
    th = th.filter(":visible");					//filter invisible columns
    t.cg = t.find("col"); 						//a table can also contain a colgroup with col elements
    t.ln = th.length;							//table length is stored
    th.each(function(i){						//iterate through the table column headers
      var c = $(this); 						//jquery wrap for the current column

      var dc = t.dc.indexOf(i)!=-1;           //is this a disabled column?
      var g = $(t.gc.append('<div class="JCLRgrip"></div>')[0].lastChild); //add the visual node to be used as grip
      g.append(dc ? "": t.opt.gripInnerHtml).append('<div class="'+SIGNATURE+'"></div>');
      if(i == t.ln-1){                        //if the current grip is the las one
        g.addClass("JCLRLastGrip");         //add a different css class to stlye it in a different way if needed
        if(t.f) g.html("");                 //if the table resizing mode is set to fixed, the last grip is removed since table with can not change
      }
      g.bind('touchstart mousedown', onGripMouseDown); //bind the mousedown event to start dragging

      if (!dc){
        //if normal column bind the mousedown event to start dragging, if disabled then apply its css class
        g.removeClass('JCLRdisabledGrip').bind('touchstart mousedown', onGripMouseDown);
      }else{
        g.addClass('JCLRdisabledGrip');
      }

      g.t = t; g.i = i; g.c = c;	c.w =c.width();		//some values are stored in the grip's node data as shortcut
      t.g.push(g); t.c.push(c);						//the current grip and column are added to its table object

      c.width(c.w).removeAttr("width");				//the width of the column is converted into pixel-based measurements
      g.data(SIGNATURE, {i:i, t:t.attr(ID), last: i == t.ln-1});	 //grip index and its table name are stored in the HTML
    });
    t.cg.removeAttr("width");	//remove the width attribute from elements in the colgroup

    t.find('td, th').not(th).not('table th, table td').each(function(){
      $(this).removeAttr('width');	//the width attribute is removed from all table cells which are not nested in other tables and dont belong to the header
    });

    syncGrips(t); 				//the grips are positioned according to the current table layout
    //there is a small problem, some cells in the table could contain dimension values interfering with the
    //width value set by this plugin. Those values are removed

  };


  /**
   * Function that places each grip in the correct position according to the current table layout
   * @param {jQuery ref} t - table object
   */
  var syncGrips = function (t){
    t.gc.width(t.w);			//the grip's container width is updated
    for(var i=0; i<t.ln; i++){	//for each column
      var c = t.c[i];
      t.g[i].css({			//height and position of the grip is updated according to the table layout
        left: c.offset().left - t.offset().left + c.outerWidth(false) + t.cs / 2 + PX,
        height: t.opt.headerOnly? t.c[0].outerHeight(false) : t.outerHeight(false)
      });
    }
  };


  /**
   * Event handler used while dragging a grip. It checks if the next grip's position is valid and updates it.
   * @param {event} e - mousemove event binded to the window object
   */
  var onGripDrag = function(e){
    if(!drag) return;
    var t = drag.t;		//table object reference
    var oe = e.originalEvent.touches;
    var ox = oe ? oe[0].pageX : e.pageX;    //original position (touch or mouse)
    var x =  ox - drag.ox + drag.l;	        //next position according to horizontal mouse position increment
    var mw = t.opt.minWidth, i = drag.i ;	//cell's min width
    var l = t.cs*1.5 + mw + t.b;
    var last = i == t.ln-1;                 			//check if it is the last column's grip (usually hidden)
    var min = i? t.g[i-1].position().left+t.cs+mw: l;	//min position according to the contiguous cells
    var max = t.f ? 	//fixed mode?
       i == t.ln-1?
          t.w-l:
          t.g[i+1].position().left-t.cs-mw:
       Infinity; 								//max position according to the contiguous cells
    x = M.max(min, M.min(max, x));				//apply bounding
    drag.x = x;	 drag.css("left",  x + PX); 	//apply position increment
    if (last) {
      var c = t.c[drag.i];					//width of the last column is obtained
      drag.w = c.w + x- drag.l;
    }
    return false; 	//prevent text selection while dragging
  };


  /**
   * Event handler fired when the dragging is over, updating table layout
   * @param {event} e - grip's drag over event
   */
  var onGripDragOver = function(e){
    d.unbind('touchend.'+SIGNATURE+' mouseup.'+SIGNATURE).unbind('touchmove.'+SIGNATURE+' mousemove.'+SIGNATURE);
    $("head :last-child").remove(); 				//remove the dragging cursor style
    if(!drag) return;
    drag.removeClass(drag.t.opt.draggingClass);		//remove the grip's dragging css-class
    if (!(drag.x - drag.l == 0)) {
      var t = drag.t;
      var cb = t.opt.onResize; 	    //get some values
      var i = drag.i;                 //column index
      var last = i == t.ln-1;         //check if it is the last column's grip (usually hidden)
      var c = t.g[i].c;               //the column being dragged

      var inc = drag.x-drag.l;

      if (last) {
        c.width(drag.w);
        c.w = drag.w;
      } else {
        c.w = c.w + inc;
        c.width(c.w);
      }

      // wait for UI to update to sync the grips
      // the calculation for the table's width happens in session.list.component.js
      // because the info column is super special
      window.setTimeout(function() {
        t.w = t.width();    // update the table with
        syncGrips(t);				// the grips are updated
      }, 300);

      if (cb) { e.currentTarget = t[0]; cb(e, c, i); }	//if there is a callback function, it is fired
    }
    drag = null;   //since the grip's dragging is over
  };


  /**
   * Event handler fired when the grip's dragging is about to start. Its main goal is to set up events
   * and store some values used while dragging.
   * @param {event} e - grip's mousedown event
   */
  var onGripMouseDown = function(e){
    var o = $(this).data(SIGNATURE);			//retrieve grip's data
    var t = tables[o.t],  g = t.g[o.i];			//shortcuts for the table and grip objects
    var oe = e.originalEvent.touches;           //touch or mouse event?
    g.ox = oe? oe[0].pageX: e.pageX;            //the initial position is kept
    g.l = g.position().left;
    g.x = g.l;

    d.bind('touchmove.'+SIGNATURE+' mousemove.'+SIGNATURE, onGripDrag ).bind('touchend.'+SIGNATURE+' mouseup.'+SIGNATURE, onGripDragOver);	//mousemove and mouseup events are bound
    h.append("<style type='text/css'>*{cursor:"+ t.opt.dragCursor +"!important}</style>"); 	//change the mouse cursor
    g.addClass(t.opt.draggingClass); 	//add the dragging class (to allow some visual feedback)
    drag = g;							//the current grip is stored as the current dragging object
    if(t.c[o.i].l) for(var i=0,c; i<t.ln; i++){ c=t.c[i]; c.l = false; c.w= c.width(); } 	//if the colum is locked (after browser resize), then c.w must be updated
    return false; 	//prevent text selection
  };

  var resizeTimeout;
  var onResize = function(){
    if (resizeTimeout) { clearTimeout(resizeTimeout); }
    resizeTimeout = window.setTimeout(function() {
      for(var t in tables){
        if( tables.hasOwnProperty( t ) ) {
          t = tables[t];
          syncGrips(t);
        }
      }
    }, 500);
  };


  //bind resize event, to update grips position
  $(window).bind('resize.'+SIGNATURE, onResize);


  /**
   * The plugin is added to the jQuery library
   * @param {Object} options -  an object that holds some basic customization values
   */
  $.fn.extend({
    colResizable: function(options) {
      var defaults = {

        //attributes:

        resizeMode: 'fit',                    //mode can be 'fit', 'flex' or 'overflow'
        draggingClass: 'JCLRgripDrag',	//css-class used when a grip is being dragged (for visual feedback purposes)
        gripInnerHtml: '',				//if it is required to use a custom grip it can be done using some custom HTML
        liveDrag: false,				//enables table-layout updating while dragging
        minWidth: 15, 					//minimum width value in pixels allowed for a column
        headerOnly: false,				//specifies that the size of the the column resizing anchors will be bounded to the size of the first row
        hoverCursor: "col-resize",  		//cursor to be used on grip hover
        dragCursor: "col-resize",  		//cursor to be used while dragging
        postbackSafe: false, 			//when it is enabled, table layout can persist after postback or page refresh. It requires browsers with sessionStorage support (it can be emulated with sessionStorage.js).
        flush: false, 					//when postbakSafe is enabled, and it is required to prevent layout restoration after postback, 'flush' will remove its associated layout data
        marginLeft: null,				//in case the table contains any margins, colResizable needs to know the values used, e.g. "10%", "15em", "5px" ...
        marginRight: null, 				//in case the table contains any margins, colResizable needs to know the values used, e.g. "10%", "15em", "5px" ...
        disable: false,					//disables all the enhancements performed in a previously colResized table
        partialRefresh: false,			//can be used in combination with postbackSafe when the table is inside of an updatePanel,
        disabledColumns: [],            //column indexes to be excluded
        removePadding: true,           //for some uses (such as multiple range slider), it is advised to set this modifier to true, it will remove padding from the header cells.

        //events:
        onDrag: null, 					//callback function to be fired during the column resizing process if liveDrag is enabled
        onResize: null					//callback function fired when the dragging process is over
      }
      var options =  $.extend(defaults, options);

      //since now there are 3 different ways of resizing columns, I changed the external interface to make it clear
      //calling it 'resizeMode' but also to remove the "fixed" attribute which was confusing for many people
      options.fixed = true;
      options.overflow = false;
      switch(options.resizeMode){
        case 'flex': options.fixed = false; break;
        case 'overflow': options.fixed = false; options.overflow = true; break;
      }

      return this.each(function() {
        init( this, options);
      });
    }
  });
})(jQuery);
