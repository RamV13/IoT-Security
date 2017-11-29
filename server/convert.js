
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
data = _.map(data, (x) => Math.trunc((x - min) * 4095.5 / (max - min)));

function int2binary(num) {
  var bits = [];
  for (var i = 0; i < 12; i++) {
    bits.push(num % 2);
    num = num >> 1;
  }
  return _.reverse(bits);
}

function binary2int(bits) {
  bits = _.reverse(bits);
  var num = 0;
  for (var i = 0; i < _.size(bits); i++) {
    num += bits[i] * Math.pow(2, i);
  }
  return num;
}

var count = 0;

data = _.map(data, (x) => {
  var bits = int2binary(x);
  var least = _.slice(bits, 6, 12);
  least.unshift(0);
  least = binary2int(least);
  var most = _.slice(bits, 0, 6);
  most.unshift(1);
  most = binary2int(most);
  return least + "," + most;
});

fs.writeFileSync(song + 's', _.reduce(data, (acc, x) => acc + x + '\n', ''));
