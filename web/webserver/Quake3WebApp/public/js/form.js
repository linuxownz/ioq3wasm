$(document).ready ( 
    function () {
        if ( $("#msg").text() != "" ) {
            $(window).scrollTop($('#msg').offset().top);
        }
        
        $(".section_label").each (
            function ( idx, obj ) {
                $("#floating-menu li:last-child ").after ( "<li onclick='doScroll(\"" + obj.id + "\")'>" + obj.textContent + "</li>" );
            }
        );

        $("#floating-menu li:last-child ").after ( "<li onclick='doScroll(\"submit_buttons\")'>Submit</li>" );
		$("#floating-nav-content").slideDown(600);
    }
)

$(document).scroll(function() {
	if ($(this).scrollTop() == 0 && 0) {
		$("#floating-nav-content").slideUp(400);
	} else {
		$("#floating-nav-content").slideDown(600);
	}
});

function doScroll ( id ) {
    $(window).scrollTop($("#" + id).offset().top - 50);
}
