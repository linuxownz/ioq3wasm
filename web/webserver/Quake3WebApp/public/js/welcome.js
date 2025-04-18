console.log('welcome');

function display_servers(responseText) {
    console.log(responseText);
}

function load_servers() {
    ajax("servers", display_servers);
}
