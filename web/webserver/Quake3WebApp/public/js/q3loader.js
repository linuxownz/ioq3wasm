//"use strict";

var Module = {
    console_pageup () {
    },

    console_key( key ) {
        const char  = String.fromCharCode(key);
        console.log ( 'console_key ' + char );

    },

    console_clear() {
        var output = document.getElementById('output');
        output.value = '';
    },

    console_set ( str ) {
        const input = document.getElementById('input');
        input.value = str;
    },

    map_change ( mapname ) {
        console.log('map_change ' + mapname);
        window.sessionStorage.setItem('mapname', mapname);
        window.location.reload();
    },

    setting_change ( cvar_name, cvar_value ) {

        obj = {};
        obj[cvar_name] = cvar_value;
        obj['rm']      = 'setting_change';

        const xhr  = new XMLHttpRequest();
        xhr.open ( 'POST', '/', true );

        xhr.onreadystatechange = () => {
            console.log ( xhr.status );
            if (xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
                // Request finished. Do processing here.
                console.log(xhr.response);
            }
        };

        xhr.setRequestHeader("Content-Type", "application/json");
        xhr.send(JSON.stringify(obj));
    },

    binding_change ( bind, binding ) {
        obj = {};
        obj['bind']    = bind;
        obj['binding'] = binding;
        obj['rm']      = 'binding_change';

        console.log(obj);

        const xhr = new XMLHttpRequest();
        xhr.open ( 'POST', '/', true );

        xhr.onreadystatechange = () => {
            console.log ( xhr.status );
            if (xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
                // Request finished. Do processing here.
                console.log(xhr.response);
            }
        };

        xhr.setRequestHeader("Content-Type", "application/json");
        xhr.send(JSON.stringify(obj));
    },

    atExitNow() {
        var canvas  = document.getElementById('canvas');
        var output  = document.getElementById('output');
        var  input  = document.getElementById('input');
        var playbut = document.getElementById('playbutton');

        document.exitPointerLock();

        input.style.visibility  = 'hidden';
        output.style.visibility = 'hidden';
        canvas.style.visibility = 'hidden';

        playbut.disabled = false;

        window.location.reload();
    },

    toggle_console( show ) {
        var canvas = document.getElementById('canvas');
        var output = document.getElementById('output');
        var  input = document.getElementById('input');

        var animationName = output.style.animationName;

        if ( ! animationName ) {
            animationName = "slideup";
        }

        if ( ! show ) {
            animationName = "slideup";
            output.addEventListener('animationend', function() {
                var input = document.getElementById('input');
                this.style.visibility  = "hidden";
                input.style.visibility = "hidden";
            }, { once : true } );

        } else if ( show ) {
            animationName = "slidedown";
            output.addEventListener('animationend', function() {
                var input = document.getElementById('input');
                document.exitPointerLock();
                this.style.visibility   = "visible";
                input.style.visibility  = "visible";
            }, { once : true } );
        }

        input.style.animationName  = animationName;
        output.style.animationName = animationName;
    },

    fade_in_canvas() {
        let canvas = document.getElementById('canvas');
        canvas.style.animationName = 'canvasfadein';

        canvas.addEventListener('animationend', function() {
            canvas.style.visibility = 'visible';

            let a = document.createElement('audio');
            a.src = '/approot/e12e0b9ae6ecad3742cfd1283528ae61';
            a.play();

        }, { once : true } );
    },

    arguments : [  ],

    noInitialRun : true,

    print: (function() {
        var element = document.getElementById('output');
        if (element) element.value = ''; // clear browser cache
        return (...args) => {
            var text = args.join(' ');
            // These replacements are necessary if you render to raw HTML
            //text = text.replace(/&/g, "&amp;");
            //text = text.replace(/</g, "&lt;");
            //text = text.replace(/>/g, "&gt;");
            //text = text.replace('\n', '<br>', 'g');
            console.log(text);
            if (element) {
                element.value += text + "\n";
                element.scrollTop = element.scrollHeight; // focus on bottom
            }
        };
    })(),
    canvas: (() => {
        var canvas = document.getElementById('canvas');

        // As a default initial behavior, pop up an alert when webgl context is lost. To make your
        // application robust, you may want to override this behavior before shipping!
        // See http://www.khronos.org/registry/webgl/specs/latest/1.0/#5.15.2
        canvas.addEventListener("webglcontextlost", (e) => { alert('WebGL context lost. You will need to reload the page.'); e.preventDefault(); }, false);

        return canvas;
    })(),
    setStatus: (text) => {
        if (!Module.setStatus.last) {
            Module.setStatus.last = { time: Date.now(), text: '' };
        }

        if (text === Module.setStatus.last.text) {
            return;
        }

        var m = text.match(/([^(]+)\((\d+(\.\d+)?)\/(\d+)\)/);
        var now = Date.now();

        if (m && now - Module.setStatus.last.time < 30) {
            return; // if this is a progress update, skip it if too soon
        }

        Module.setStatus.last.time = now;
        Module.setStatus.last.text = text;

        if (m) {
            text = m[1];
        }

        console.log(text);
    },
    totalDependencies: 0,
    monitorRunDependencies: (left) => {
        left = left || 0;
        this.totalDependencies = Math.max(this.totalDependencies || 0, left);
        Module.setStatus(left ? 'Preparing... (' + (this.totalDependencies-left) + '/' + this.totalDependencies + ')' : 'All downloads complete.');
    }
};

window.onerror = (event) => {
    console.log ( event );
    // TODO: do not warn on ok events like simulating an infinite loop or exitStatus
    Module.setStatus('Exception thrown, see JavaScript console');
    Module.setStatus = (text) => {
        if (text) console.error('[post-exception status] ' + text);
    };
};

/*
var Module = {
    print: function(args) {
        var text = args.join(' ');
        console.log(text);
    },
    canvas: function () {
        var canvas = document.getElementById('canvas');

        // As a default initial behavior, pop up an alert when webgl context is lost. To make your
        // application robust, you may want to override this behavior before shipping!
        // See http://www.khronos.org/registry/webgl/specs/latest/1.0/#5.15.2
        canvas.addEventListener("webglcontextlost", (e) => { alert('WebGL context lost. You will need to reload the page.'); e.preventDefault(); }, false);

        return canvas;
    },
    setStatus: function(text) {
        if (!Module.setStatus.last) Module.setStatus.last = { time: Date.now(), text: '' };
        if (text === Module.setStatus.last.text) return;
        var m = text.match(/([^(]+)\((\d+(\.\d+)?)\/(\d+)\)/);
        var now = Date.now();
        if (m && now - Module.setStatus.last.time < 30) return; // if this is a progress update, skip it if too soon
        Module.setStatus.last.time = now;
        Module.setStatus.last.text = text;
        console.log(text);
    },
    totalDependencies: 0,
    monitorRunDependencies: function(left) {
        this.totalDependencies = Math.max(this.totalDependencies, left);
        Module.setStatus(left ? 'Preparing... (' + (this.totalDependencies-left) + '/' + this.totalDependencies + ')' : 'All downloads complete.');
    }
};

window.onerror = (event) => {
    console.log ( event ); // asdfsdf

    // TODO: do not warn on ok events like simulating an infinite loop or exitStatus
    Module.setStatus('Exception thrown, see JavaScript console');
    Module.setStatus = (text) => {
        if (text) console.error('[post-exception status] ' + text);
    };
};*/

