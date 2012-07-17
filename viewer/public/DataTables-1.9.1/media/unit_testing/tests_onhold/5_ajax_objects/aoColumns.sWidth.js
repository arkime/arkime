// DATA_TEMPLATE: empty_table
oTest.fnStart( "aoColumns.sWidth" );

/* NOTE - we need to disable the auto width for the majority of these test in order to preform 
 * these tests as the auto width will convert the width to a px value. We can do 'non-exact' tests
 * with auto width enabled however to ensure it scales columns as required
 */

$(document).ready( function () {
	/* Check the default */
	var oTable = $('#example').dataTable( {
		"sAjaxSource": "../../../examples/ajax/sources/objects.txt",
		"bAutoWidth": false,
		"aoColumns": [
			{ "mDataProp": "engine" },
			{ "mDataProp": "browser", "sWidth": '40%' },
			{ "mDataProp": "platform" },
			{ "mDataProp": "version" },
			{ "mDataProp": "grade" }
		]
	} );
	var oSettings = oTable.fnSettings();
	
	oTest.fnWaitTest( 
		"With auto width disabled the width for one column is appled",
		null,
		function () { return $('#example thead th:eq(1)')[0].style.width == "40%"; }
	);
	
	oTest.fnWaitTest( 
		"With auto width disabled the width for one column is appled",
		function () {
			oSession.fnRestore();
			oTable = $('#example').dataTable( {
				"sAjaxSource": "../../../examples/ajax/sources/objects.txt",
				"bAutoWidth": false,
				"aoColumns": [
					{ "mDataProp": "engine" },
					{ "mDataProp": "browser" },
					{ "mDataProp": "platform", "sWidth": '20%' },
					{ "mDataProp": "version", "sWidth": '30%' },
					{ "mDataProp": "grade" }
				]
			} );
		},
		function () {
			var bReturn =
				$('#example thead th:eq(2)')[0].style.width == "20%" &&
				$('#example thead th:eq(3)')[0].style.width == "30%";
			return bReturn;
		}
	);
	
	
	oTest.fnWaitTest( 
		"With auto width, it will make the smallest column the largest with percentage width given",
		function () {
			oSession.fnRestore();
			oTable = $('#example').dataTable( {
				"sAjaxSource": "../../../examples/ajax/sources/objects.txt",
				"aoColumns": [
					{ "mDataProp": "engine" },
					{ "mDataProp": "browser" },
					{ "mDataProp": "platform" },
					{ "mDataProp": "version", "sWidth": '40%' },
					{ "mDataProp": "grade" }
				]
			} );
		},
		function () {
			var anThs = $('#example thead th');
			var a0 = anThs[0].offsetWidth;
			var a1 = anThs[1].offsetWidth;
			var a2 = anThs[2].offsetWidth;
			var a3 = anThs[3].offsetWidth;
			var a4 = anThs[4].offsetWidth;
			
			if ( a3>a0 && a3>a1 && a3>a2 && a3>a4 )
				return true;
			else
				return false;
		}
	);
	
	
	oTest.fnComplete();
} );