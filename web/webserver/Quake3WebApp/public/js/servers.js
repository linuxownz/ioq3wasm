"use strict";

function filterServersByGameType(event) {
    var gt = event.srcElement.dataset.gametype;
    console.log(gt);

    var servers = document.querySelectorAll('.server');
    for ( var i = 0 ; i <  servers.length ; i++ ) {
        var server = servers[i];
        var sgt = server.dataset.gametype;

        if ( gt === '' || gt === sgt ) {
            server.style.display    = 'flex';
            server.style.visibility = 'visible';
        } else {
            server.style.display    = 'none';
            server.style.visibility = 'hidden';
        }
    }
}

window.addEventListener ( 'load', function ( event ) {
    var filter_buttons = document.getElementsByClassName('filter-button');
    for ( var i = 0 ; i < filter_buttons.length ; i++ ) {
        var button = filter_buttons[i];
        button.addEventListener('click', filterServersByGameType, false );
    }
});
