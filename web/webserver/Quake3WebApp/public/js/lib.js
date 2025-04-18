function ajax ( action, callback ) {
    // not used?
    debugger;
    const request = new XMLHttpRequest();
    request.onreadystatechange = () => {
        if ( request.readyState === 4 ) {
            callback ( request);
        }
    }
    request.open( "POST", "/" );
    request.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
    request.send( "rm=ajax&action=" + action );
}

// COLOR_BLACK     '0'     
// COLOR_RED       '1'
// COLOR_GREEN     '2'
// COLOR_YELLOW    '3'
// COLOR_BLUE      '4'
// COLOR_CYAN      '5'
// COLOR_MAGENTA   '6'
// COLOR_WHITE     '7'


function colorText( text ) {

    let newText = document.createElement('span');

    let color      = 'color_white';
    let textfrag   = '';
    let colorstart = false;
    let incolor    = false;

    for ( let i = 0 ; i < text.length ; i++ ) {
        let chr = text[i];

        if ( chr == '^' ) {
            colorstart = true;
            incolor    = false; 

            if ( textfrag.length > 0 ) {
                let s = document.createElement('span');
                let t = document.createTextNode(textfrag);

                s.className = color;
                s.appendChild(t);

                newText.appendChild(s);
                textfrag = '';
            }

        } else if ( colorstart ) {
            switch ( chr ) {
                case '0': color = 'color_black';   break;
                case '1': color = 'color_red';     break;
                case '2': color = 'color_green';   break;
                case '3': color = 'color_yellow';  break;
                case '4': color = 'color_blue';    break;
                case '5': color = 'color_cyan';    break;
                case '6': color = 'color_magenta'; break;
                case '7': color = 'color_white';   break;
                case '^': color = 'color_white';   break;
                default: 
                    textfrag  += '^';
                    colorstart = false;
                    incolor    = false;
                    textfrag  += chr;
                    break;
            }

            colorstart = false;
            incolor    = true;

        } else {
            textfrag += chr;
        }
    }

    let s = document.createElement('span');
    let t = document.createTextNode(textfrag);

    s.className = color;

    s.appendChild(t);
    newText.appendChild(s);

    return newText;
}













