var fs = require('fs');
var path = require('path');
var watchSeenOne = 0;
var filenameOne = 'watch.txt';
var filepathOne = path.join('/tmp', filenameOne);
var testsubdir = path.join('/tmp', 'testsubdir');


fs.writeFileSync(filepathOne, 'hello');
var change = function () {
  var watcher = fs.watch(filepathOne, function (event, filename) {
    console.log('change event:'+event);
    console.log('change filename:'+filename);
    try {
      watcher.close();
    } catch (e) {

    }
    watchSeenOne += 1;
  });
};

change();
setTimeout(function() {
  fs.writeFileSync(filepathOne, 'world');
}, 1000);


try {
  fs.mkdirSync(testsubdir, 0700);
}
catch (e) {

}
var rename = function () {
  var watcher = fs.watch(testsubdir, function (event, filename) {
    console.log('add event:'+event);
    console.log('add filename:'+filename);
    if (watchSeenOne > 4) {
       watcher.close();
    }
    watchSeenOne += 1;
  });
};
rename();
setTimeout(function() {
  var fd = fs.openSync(testsubdir + '/hi.txt', 'w');
  fs.closeSync(fd);
}, 5000);

