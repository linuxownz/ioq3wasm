window.addEventListener ( "load", (event) => {
    //console.log ( "window loaded" );

    if ( typeof instance_id !== 'undefined' && instance_id !== '' ) {
        if ( Module ) {

            let mapname2 = window.sessionStorage.getItem('mapname');
            let autostart = false;

            if ( mapname2 ) {
                autostart = true;
                window.sessionStorage.removeItem('mapname');
                Module.arguments.push('map='  + mapname2);
            } else {
                Module.arguments.push('map='  + mapname);
            }

            let htmloverlays = document.querySelectorAll('.html_overlay');
            if ( htmloverlays ) {
                Array.from(htmloverlays).forEach( htmloverlay => {
                    htmloverlay.addEventListener('click', function() {
                        if ( document.activeElement != canvas ) {
                            canvas.requestPointerLock();
                        }
                    });
                });
            }

            let ds = document.querySelectorAll('#escape_menu div');
            Array.from(ds).forEach( esc_div => {
                esc_div.addEventListener('click', function( ev, a, b) {
                    Array.from(htmloverlays).forEach( htmloverlay => {
                        htmloverlay.style.visibility = 'hidden';
                        htmloverlay.style.display = 'none';
                    });

                    switch ( ev.srcElement.textContent ) {
                        case 'Join Red'        : Module._Com_FromJSCommand(1); break;
                        case 'Join Blue'       : Module._Com_FromJSCommand(2); break;
                        case 'Join Team Free'  : Module._Com_FromJSCommand(3); break;
                        case 'Join Spectators' : Module._Com_FromJSCommand(4); break;
                        case 'Exit game': Module._Com_Quit_f(); break;
                    }
                });
            });

            Module.arguments.push('port=' + port   );

            Module['onRuntimeInitialized'] = function() {
                //console.log("onRuntimeInitialized");
                if ( autostart ) {
                    //console.log('calling main');
                    canvas.style.visibility = "visible";
                    callMain(Module.arguments);
                }
            };

            Module['postRun'] = () => { 
                complete();
            };
        }
    }

    let playbutton = document.getElementById('playbutton');
    playbutton.addEventListener('click', function ( e, f ) {
        play();
    });
});

function play() {
    //console.log('starting up...');
    document.getElementById('playbutton').disabled = true;
    //document.getElementById('canvas').style.visibility = 'visible';
    callMain(Module.arguments);
    //console.log('after main');
}

function complete() {
    //console.log('postRun complete TODO');
    console.log('complete');
}
