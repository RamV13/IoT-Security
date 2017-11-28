
var fs = require('fs');
var wav = require('node-wav');
var _ = require('lodash');

if (_.size(process.argv) < 3) {
  return console.error('Missing song file parameter');
}

var song = process.argv[2];

var file;
try {
  file = fs.readFileSync(song);
} catch (err) {
  return console.error('failed to read song: ' + song);
}
if (!file) return console.error('failed to read song: ' + song);
var result;
try {
  result = wav.decode(file);
} catch (err) {
  return console.error('failed to decode song: ' + song);
}
if (!result) return console.error('failed to decode song: ' + song);
var data = result.channelData[0];
var min = _.min(data);
var max = _.max(data);
data = _.map(data, (x) => Math.trunc((x - min) * 4095.5 * (max - min)));
// TODO bitwise OR with DAC CS
fs.writeFileSync(song + 's', _.reduce(data, (acc, x) => acc + (acc == '' ? '' : ',') + x, ''));
