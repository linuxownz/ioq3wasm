#include <emscripten.h>

EM_JS(int, console_log,(const char *a, int b), { // dont think this is used
    let x = UTF8ArrayToString(Module.HEAP8, a, b);
    console.log(x);
});

EM_JS(void, toggle_console,(short show), {
    Module.toggle_console(show);
});

EM_JS(void, console_pageup, (), {
    Module.console_pageup();
});

EM_JS(void, console_set,( char * str, short length, short cursorpos ), {
    //console.log ( 'console_set' );
    Module.console_set ( UTF8ToString(str) );
});

EM_JS(void, console_clear,(), {
    Module.console_clear();
});

EM_JS(void, setting_change,(const char *name,const char *value), {
    //console.log('setting_change');
    Module.setting_change(UTF8ToString(name), UTF8ToString(value));
});

EM_JS(void, binding_change,(const char *name,const char *value), {
    //console.log('binding_change');
    Module.binding_change(UTF8ToString(name), UTF8ToString(value));
});

EM_JS(void, load_progress,(const char *_msg, int _progress), {
    let msg = UTF8ToString(_msg);

    //console.log('load progress ....');
    //console.log(msg + ' ' + _progress);

    let prog_container = document.getElementById('progress_container');
    if ( prog_container ) {
        prog_container.style.visibility = 'visible';
        prog_container.style.display = 'block';
    }

    let progress = document.getElementById('progress');
    if ( progress ) {
        progress.value = _progress;
    }

    let prog_label = document.getElementById('prog_label');
    if ( prog_label ) {
       prog_label.innerText = msg;
    }
});

EM_JS(void, show_canvas,(), {
    Module.fade_in_canvas();
});

EM_JS(void, toggle_escape_menu,(void), {
    //console.log('toggle escape menu ');

    let x = document.getElementsByClassName('html_overlay')[1];
    let y = document.getElementById('escape_menu');

    let v = x.style.visibility;
    let d = x.style.display;

    let h = false;
    if ( v == 'hidden' ) {
        v = 'visible';
        d = 'block';
        h = true;

    } else {
        v = 'hidden';
        d = 'none';
    }

    x.style.visibility = v;
    x.style.display    = d;

    y.style.visibility = v;
    y.style.display    = d;

    if ( h ) {
        //console.log('hiding html overy 0');
        let x2 = document.getElementsByClassName('html_overlay')[0];
        x2.style.visibility = 'hidden';
        x2.style.display = 'none';
    }
});

EM_JS(void, show_scores,(int show, char *_json), {
    let jsonobj = null;

    let quadimg       = '/approot/d5cafa5ffbf89c73d82bbb2bd42fe7a1';
    let regenimg      = '/approot/4df31a689e336673feb37f1dee40a608';
    let battlesuitimg = '/approot/018118b267994322301bddc46b0a6815';
    let hasteimg      = '/approot/a5d826f879c6090c783740c6ae5e98f6';
    let invisimg      = '/approot/3bb5a84c6a6888b5c4155de5c9daf11a';
    let flightimg     = '/approot/e8fc13b786aa459c0c4fd6b67f8fb529';
    let redflagimg    = '/approot/7aabe3a263650caf708002a1c102b691';
    let blueflagimg   = '/approot/df7bd3b5b9e5948cb494ccda3d6b1be4';

    const createImg = ( imgname ) => { let x = document.createElement('img'); x.src = imgname; return x; };

    if ( show && _json ) {
        let json = UTF8ToString(_json);
        try {
            jsonobj = JSON.parse(json);
        } catch ( e ) {
            console.log(e);
        } finally {
            //console.log(jsonobj);
        }

        let followClientNum = jsonobj.clientNum;

        if ( jsonobj.g_gametype < 3 ) { // non team play
            jsonobj.playersInfo.sort( (a, b) => b.score - a.score);

            for ( let x = 1 ; x < 3 ; x++ ) {
                let tbody = document.querySelector("#nonteam table:nth-child(" + x + ") tbody");

                if ( tbody ) {

                    while (tbody.firstChild) {
                        tbody.removeChild(tbody.firstChild);
                    }

                    for ( let i = 0 ; i < jsonobj.playersInfo.length; i++ ) {
                        let pi = jsonobj.playersInfo[i];

                        if ( x == 1 && pi.team >= 3 || x == 2 && pi.team < 3 ) {
                            continue;
                        }

                        let tr = document.createElement("tr");

                        if ( followClientNum == pi.clientNum ) {
                            tr.className = "followHilight";
                        }

                        let t1 = document.createElement("td");
                        let t2 = document.createElement("td");
                        let t3 = document.createElement("td");
                        let t4 = document.createElement("td");
                        let t5 = document.createElement("td");

                        if ( pi.powerups > 0 ) {
                            let pu = pi.powerups;

                            if ( pu &   2 ) t1.appendChild(createImg(quadimg));
                            if ( pu &   4 ) t1.appendChild(createImg(battlesuitimg));
                            if ( pu &   8 ) t1.appendChild(createImg(hasteimg));
                            if ( pu &  16 ) t1.appendChild(createImg(invisimg));
                            if ( pu &  32 ) t1.appendChild(createImg(regenimg));
                            if ( pu &  64 ) t1.appendChild(createImg(flightimg));
                            if ( pu & 128 ) t1.appendChild(createImg(redflagimg));  // never happen in non team play
                            if ( pu & 256 ) t1.appendChild(createImg(blueflagimg)); // never happen in non team play
                        }

                        t2.appendChild(colorText(pi.name));
                        t3.appendChild(document.createTextNode(pi.ping));
                        t4.appendChild(document.createTextNode(pi.time));
                        t5.appendChild(document.createTextNode(pi.score));

                        tr.appendChild(t1);
                        tr.appendChild(t2);
                        tr.appendChild(t3);
                        tr.appendChild(t4);
                        tr.appendChild(t5);

                        tbody.appendChild(tr);
                    }
                }
            }

            let nonteam = document.getElementById('nonteam');
            nonteam.style.visibility = 'visible';
            nonteam.style.display = 'block';

        } else { // team play
            let redscore = document.getElementById('red_score');
            let bluscore = document.getElementById('blue_score');

            redscore.textContent = jsonobj.red_score;
            bluscore.textContent = jsonobj.blue_score;

            let tbody = document.querySelector("#team table tbody");

            while (tbody.firstChild) {
                tbody.removeChild(tbody.firstChild);
            }

            let red_team  = [];
            let blue_team = [];
            let free_team = [];

            for ( let i = 0 ; i < jsonobj.playersInfo.length; i++ ) {
                let pi = jsonobj.playersInfo[i];

                if ( pi.team == 1 ) {
                    red_team.push(pi);
                } else if ( pi.team == 2 ) {
                    blue_team.push(pi);
                } else {
                    free_team.push(pi);
                }
            }

            red_team.sort( (a, b)  => b.score - a.score).reverse();
            blue_team.sort( (a, b) => b.score - a.score).reverse();
            free_team.sort( (a, b) => b.score - a.score).reverse();

            let maxplayers = Math.max(red_team.length, blue_team.length);

            for ( let x = 0 ; x < maxplayers ; x++ ) {
                let r = red_team.pop();
                let b = blue_team.pop();

                if ( r || b ) {
                    let tr = document.createElement('tr');

                    let t1 = document.createElement("td");
                    let t2 = document.createElement("td");
                    let t3 = document.createElement("td");
                    let t4 = document.createElement("td");
                    let t5 = document.createElement("td");

                    if ( r ) {
                        if ( r.powerups > 0 ) {
                            let pu = r.powerups;

                            if ( pu &   2 ) t1.appendChild(createImg(quadimg));
                            if ( pu &   4 ) t1.appendChild(createImg(battlesuitimg));
                            if ( pu &   8 ) t1.appendChild(createImg(hasteimg));
                            if ( pu &  16 ) t1.appendChild(createImg(invisimg));
                            if ( pu &  32 ) t1.appendChild(createImg(regenimg));
                            if ( pu &  64 ) t1.appendChild(createImg(flightimg));
                            if ( pu & 128 ) t1.appendChild(createImg(redflagimg));
                            if ( pu & 256 ) t1.appendChild(createImg(blueflagimg));
                        }

                        t2.appendChild(colorText(r.name));
                        t3.appendChild(document.createTextNode(r.ping));
                        t4.appendChild(document.createTextNode(r.time));
                        t5.appendChild(document.createTextNode(r.score));

                        if ( r.clientNum == followClientNum ) {
                            t1.className = "followHilight";
                            t2.className = "followHilight";
                            t3.className = "followHilight";
                            t4.className = "followHilight";
                            t5.className = "followHilight";
                        }
                    }

                    let sep = document.createElement("td");

                    let t6 = document.createElement("td");
                    let t7 = document.createElement("td");
                    let t8 = document.createElement("td");
                    let t9 = document.createElement("td");
                    let t10 = document.createElement("td");

                    if ( b ) {

                        if ( b.powerups > 0 ) {
                            let pu = b.powerups;

                            if ( pu &   2 ) t6.appendChild(createImg(quadimg));
                            if ( pu &   4 ) t6.appendChild(createImg(battlesuitimg));
                            if ( pu &   8 ) t6.appendChild(createImg(hasteimg));
                            if ( pu &  16 ) t6.appendChild(createImg(invisimg));
                            if ( pu &  32 ) t6.appendChild(createImg(regenimg));
                            if ( pu &  64 ) t6.appendChild(createImg(flightimg));
                            if ( pu & 128 ) t6.appendChild(createImg(redflagimg));
                            if ( pu & 256 ) t6.appendChild(createImg(blueflagimg));
                        }

                        t7.appendChild(colorText(b.name));
                        t8.appendChild(document.createTextNode(b.ping));
                        t9.appendChild(document.createTextNode(b.time));
                        t10.appendChild(document.createTextNode(b.score));

                        if ( b.clientNum == followClientNum ) {
                            t6.className = "followHilight";
                            t7.className = "followHilight";
                            t8.className = "followHilight";
                            t9.className = "followHilight";
                            t10.className = "followHilight";
                        }
                    }

                    tr.appendChild(t1);
                    tr.appendChild(t2);
                    tr.appendChild(t3);
                    tr.appendChild(t4);
                    tr.appendChild(t5);

                    tr.appendChild(sep);

                    tr.appendChild(t6);
                    tr.appendChild(t7);
                    tr.appendChild(t8);
                    tr.appendChild(t9);
                    tr.appendChild(t10);

                    tbody.appendChild(tr);
                }
            }

            let spec = document.querySelector("#team table:nth-child(2) tbody");

            while (spec.firstChild) {
                spec.removeChild(spec.firstChild);
            }

            for ( let i = 0 ; i < free_team.length ; i++ ) {
                let ft = free_team[i];

                let tr = document.createElement('tr');

                let t1 = document.createElement("td");
                let t2 = document.createElement("td");
                let t3 = document.createElement("td");
                let t4 = document.createElement("td");

                t1.appendChild(colorText(ft.name));
                t2.appendChild(document.createTextNode(ft.ping));
                t3.appendChild(document.createTextNode(ft.time));
                t4.appendChild(document.createTextNode(ft.score));

                tr.appendChild(t1);
                tr.appendChild(t2);
                tr.appendChild(t3);
                tr.appendChild(t4);

                spec.appendChild(tr);
            }

            let team = document.getElementById('team');
            team.style.visibility = 'visible';
            team.style.display = 'table';

            let spectbl = document.querySelector("#team table:nth-child(2)");
            if ( free_team.length == 0 ) {
                spectbl.style.visibility = 'hidden';
                spectbl.style.display = 'none';
            } else {
                spectbl.style.visibility = 'visible';
                spectbl.style.display = 'table';
            }
        }
    }

    let x = document.getElementsByClassName('html_overlay')[0];
    let v = 'hidden';
    let d = 'none';

    if ( show ) {
        v = 'visible';
        d = 'block';
    }

    x.style.visibility = v;
    x.style.display    = d;
});

