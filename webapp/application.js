 
/* Data storage */
localforage.config({
    driver      : localforage.INDEXEDDB,
    version     : 1.0,
    storeName   : 'Arctic', // Should be alphanumeric, with underscores.
    description : 'Web app datastore for arctic tracker'
});


var datastore = localforage.createInstance({
  name: "ArcticTracker"
});
CONFIG.datastore = datastore; 




let selectedWidget = null;


/*
 * Show widget. 
 */ 
function show(id) {
    return ()=> {
        var x = pol.widget.get(id);
        x.activate( $('#widget')[0] );
        selectedWidget = x;
    }
}

setTimeout(show("core.keySetup"), 400);


function nextTracker() {
    pol.widget.get("core.keySetup").selectNext();
    if (selectedWidget.onActivate) 
        selectedWidget.onActivate();
}


function prevTracker() {
    pol.widget.get("core.keySetup").selectPrev();
    if (selectedWidget.onActivate)
        selectedWidget.onActivate();
}


function isOpen() {
    let keys = pol.widget.get("core.keySetup"); 
    if (keys.getSelected() == null || keys.getSelectedSrv() == null)
        return false; 
    
    return keys.getSelectedSrv().key!=null && keys.isAuth()
}


function getSelectedId() {
    let keys = pol.widget.get("core.keySetup");
    if (keys.getSelected() == null)
        return "NONE";
    return keys.getSelected().id;
}


function isSel(x) {
    if (selectedWidget != null && selectedWidget.classname==x)
        return ".sel";
    else
        return "";
}



/* Main Menu */
menu = {
    view: function() {
        return m("div.menu", [ 
            m("img",  {onclick: show("core.keySetup"), 
                 src: (isOpen() ? "img/unlocked.png" : "img/locked.png") }),
            
            m("span#idselect", [
                getSelectedId(), nbsp,
                m("img", {src:"img/back.png", id: "fwd", onclick: prevTracker}),
                m("img", {src:"img/forward.png", id: "fwd", onclick: nextTracker}),
            ]),
            
            m("span#buttons", [
                m("span"+isSel("core.statusInfo"),  {onclick: show("core.statusInfo")},   "Status"),
                m("span"+isSel("core.wifiSetup"),   {onclick: show("core.wifiSetup")},    "Wifi"),   
                m("span"+isSel("core.aprsSetup"),   {onclick: show("core.aprsSetup")},    "Aprs"),
                m("span"+isSel("core.digiSetup"),   {onclick: show("core.digiSetup")},    "Digi/Igate"), 
                m("span"+isSel("core.trklogSetup"), {onclick: show("core.trklogSetup")},  "Trklog"), 
            ]),

        ])
    }
};


m.mount($("div#heading")[0], menu);
m.redraw();
