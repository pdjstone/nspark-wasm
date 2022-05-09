importScripts('nspark.js');

let nsparkModule = null;
let currentTid = 0;
let currentArchive = '';
let currentDir = '';
let completedFiles = [];
let unpackQueue = [];

createNSparkModule(
    {
        noInitialRun:true, 
    }).then(async function(Module) {
    nsparkModule = Module;
    Module.FS.trackingDelegate['onCloseFile'] = function(path) {
        if (path == currentArchive)
            return;
        if (completedFiles.length > 0) {
            notifyFile(completedFiles.pop());
        }
        completedFiles.push(path);
      
    } 
    doNext();
});

function notifyFile(path) {
    let f = nsparkModule.FS.analyzePath(path);
    let buf = f.object.contents.buffer;
    self.postMessage({
        tid: currentTid, 
        path: path.substr(currentDir.length+1), 
        timestamp: f.object.timestamp, 
        buf: buf}, [buf]);
    nsparkModule.FS.unlink(path);
}

function doNext() {
    if (!nsparkModule || unpackQueue.length == 0) { 
        return;
    }
    while (unpackQueue.length > 0) {
        let item = unpackQueue.shift();
        let tid = item.tid;
        let buf = item.buf;

        let filename = tid + '.arc';
        let unpackdir = '/unpack' + tid;

        currentTid = tid;
        currentArchive = unpackdir+'/'+filename;
        currentDir = unpackdir;
        completedFiles = [];

        nsparkModule.FS.mkdir(unpackdir)
        nsparkModule.FS.createDataFile(unpackdir, filename, new Uint8Array(buf), true, true);
        nsparkModule.FS.chdir(unpackdir);
        nsparkModule.callMain(['-u', '-T', filename]);
        while (completedFiles.length > 0) {
            notifyFile(completedFiles.pop());
        }
        nsparkModule.FS.unlink(currentArchive);
        self.postMessage({tid:tid, done:true});
    }
}

self.onmessage = function(msg) {
    unpackQueue.push(msg.data);
    doNext();
}