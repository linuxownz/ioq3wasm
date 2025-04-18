"use strict";

var requestInFlight = false;

function updateCvar( event ) {

    if ( requestInFlight ) {
        return false;
    }

    console.log ( event.srcElement.name );
    requestInFlight = true;

    var obj = {};

    obj.action = 'updateCvar';
    obj.cvar   = event.srcElement.name;
    obj.value  = event.srcElement.value;
    obj.rm     = 'ajax';

    const request = new XMLHttpRequest();

    request.open("POST", "/", true);
    request.setRequestHeader("Content-Type", "application/json;charset=UTF-8");

    request.onreadystatechange = () => {
        if (request.readyState === XMLHttpRequest.DONE ) {
            if ( request.status === 200) {
                updateCvarCallback( request, obj);
            } else {
                updateCvarError( request, obj);
            }
        }
    };

    request.send(JSON.stringify(obj));
    requestInFlight = true;

    return false;
}

function updateCvarCallback ( xhr, obj ) {
    requestInFlight = false;

    console.log ( xhr );
    console.log ( obj );
}

function updateCvarError ( xhr, obj ) {
    alert('cvar update error');
    requestInFlight = false;
}

function updateBind ( info, key, mouse ) {
    if ( requestInFlight ) {
        console.log ( 'requestInFlight' );
        return false;
    }

    console.log ( info );

    // update and on success
    //   find out if its already bound.
    //     if bound somewhere else, remove that key from other bind
    //     add key to bind

    var obj = info;

    obj.action  = 'updateBind';
    obj.key     = key;
    obj.mouse   = mouse;
    obj.rm      = 'ajax';

    const request = new XMLHttpRequest();

    request.open("POST", "/", true);
    request.setRequestHeader("Content-Type", "application/json;charset=UTF-8");

    request.onreadystatechange = () => {
        if (request.readyState === XMLHttpRequest.DONE ) {
            if ( request.status === 200) {
                // Request finished. Do processing here.
                updateBindCallback( request, obj);
            } else {
                updateBindError( request, obj);
            }
        }
    };

    if ( key && key.toUpperCase() == 'BACKSPACE' ) {
        info.src.value = '';
    }

    request.send(JSON.stringify(obj));
    requestInFlight = true;
    return false;
}

function updateBindError ( xhr, obj ) {
    requestInFlight = false;
    alert('failed to update bind');
}

function updateBindCallback ( xhr, obj ) {
    requestInFlight = false;

    var json = xhr.response;

    try {
        var json = JSON.parse ( json );
    } catch ( e ) {
        console.log(e);
    };

    if ( json.update_player_bind != 1 ) {
        console.log ( 'failed to update bind' );
        return false;
    }

    var src  = obj.src;
    var name = obj.name;
    var key  = obj.key;

    // find key in other inputs and remove
    var inputs = document.querySelectorAll('.bind-input');
    for ( var i = 0 ; i < inputs.length ; i++ ) {
        var input = inputs[i];
        var val = input.value;

        var a = val.split(' ');
        var idx = a.indexOf(key);
        if ( idx >= 0 ) {
            a.splice(idx, 1);
            input.value = a.join(' ');
        }
    }

    if ( json[name] ) {
        src.value = json[name].join(' ');
    }

    var dialog = document.getElementById('bindDialog');
    dialog.close();

    return false;
}

var srcInfo = {};

function showBindKeyAcceptModal (event) {
    var src   = event.srcElement;

    srcInfo = {
          "src" : src,      // element to update with new key binding
         "name" : src.name,
        "value" : src.value,
        "title" : src.title,
    };

    console.log ( srcInfo.name + ' ' + srcInfo.value + ' ' + srcInfo.title );

    var bindaction = document.getElementById('bindaction');
    var bindkeys   = document.getElementById('bindkeys');

    while ( bindaction.hasChildNodes() ) {
        bindaction.removeChild(bindaction.childNodes[0]);
    }

    while ( bindkeys.hasChildNodes() ) {
        bindkeys.removeChild(bindkeys.childNodes[0]);
    }

    bindaction.appendChild(document.createTextNode( srcInfo.title ));
    bindkeys.appendChild(document.createTextNode( srcInfo.value.toUpperCase() ));

    var dialog     = document.getElementById('bindDialog');
    var keyboard   = document.getElementById('bindDialogKeyboard');
    var mouse      = document.getElementById('bindDialogMouse');

    keyboard.addEventListener('click', function(event) {
        var kb = document.getElementById('keyboard');
        kb.style.visibility = 'visible';

        var dc = document.getElementById('bindDialogContent');
        dc.style.visibility = 'hidden';

        event.preventDefault();
        return false;
    }, {once : true});

    dialog.addEventListener('close', function(event) {
        var kb = document.getElementById('keyboard');
        kb.style.visibility = 'hidden';

        event.preventDefault();
        return false;
    }, {once : true});

    dialog.addEventListener('keypress', function (event) {
        updateBind ( srcInfo, event.key, null );
        event.preventDefault();
        return false;
    }, {once : true});

    dialog.addEventListener('keydown', function (event) {
        if ( event.key == 'Backspace' || event.key == 'Delete' ) {
            updateBind ( srcInfo, event.key, null );
            event.preventDefault();
            return false;
        }
    });

    mouse.addEventListener('mouseup', function (event) {
        var exitButton = document.getElementById('bindDialogExit');
        var kbButton   = document.getElementById('bindDialogKeyboard');
        var msButton   = document.getElementById('bindDialogMouse');
        var srcButton  = event.srcElement;

        if ( srcButton === exitButton ) {
            return;
        }

        // 0: Main button (usually the left button)
        // 1: Auxiliary button (usually the middle or wheel button)
        // 2: Secondary button (usually the right button)
        // 3: Fourth button (typically "Browser Back")
        // 4: Fifth button (typically "Browser Forward")

        var mouse = 'MOUSE1';

        if ( srcButton === msButton ) {
            switch (event.button) {
                case 0:
                    mouse = "MOUSE1";
                    break;
                case 1:
                    mouse = "MOUSE3";
                    break;
                case 2:
                    mouse = "MOUSE2";
                    break;
                case 3:
                    mouse = "MOUSE4";
                    break;
                case 4:
                    mouse = "MOUSE5";
                    break;

                default:
                    console.log ( 'unknown button' );
                    console.log ( event.button );
                    //mouse = 'Unknown button';
            }

            updateBind ( srcInfo, null, mouse );
        }

        event.preventDefault();
        return false;
    }, {once : true});

    mouse.addEventListener('contextmenu', function (event) {
        console.log ('update setting RIGHTMOUSE');
        // console.log(event.key); console.log(event);

        updateBind ( srcInfo, null, 'MOUSE2' );

        event.preventDefault();
        return false;
    }, {once : true});

    mouse.addEventListener('wheel', function (event) {
        //console.log ('update setting MouseWheel');
        //console.log(event); 

        if ( event.deltaY < 0 ) {
            updateBind ( srcInfo, null, 'MWHEELUP' );
        } else {
            updateBind ( srcInfo, null, 'MWHEELDOWN' );
        }

        event.preventDefault();
        return false;
    }, {once : true});


    var keys = document.querySelectorAll('#keyboard div.key');
    for ( var i = 0 ; i < keys.length ; i++ ) {
        var key = keys[i];
        key.addEventListener('click', function (event) {
            updateBind ( srcInfo, event.srcElement.dataset.key, null );
            var keyboard = document.getElementById('keyboard');
            keyboard.style.visibility = 'hidden';
            dialog.close();
            event.preventDefault();
            return false;
        }, {once : true});
    }

    var kb = document.getElementById('keyboard');
    kb.style.visibility = 'hidden';

    var dc = document.getElementById('bindDialogContent');
    dc.style.visibility = 'visible';

    dialog.showModal();

    return true;
}

window.addEventListener ( 'load', function ( event ) {
    var bind_inputs = document.getElementsByClassName('bind-input');
    for ( var i = 0 ; i < bind_inputs.length ; i++ ) {
        var input = bind_inputs[i];
        input.addEventListener('click', showBindKeyAcceptModal, false );
    }

    var cvar_inputs = document.getElementsByClassName('cvar-input');
    for ( var i = 0 ; i < cvar_inputs.length ; i++ ) {
        var input = cvar_inputs[i];
        input.addEventListener('change', updateCvar, false );
    }

});


