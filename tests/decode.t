use Test::More tests => 12;
use ArkimeTest;
use JSON;
use strict;

# viewer/decode.js parses captured traffic, which is attacker controlled.
# Driven at the decode layer, so this needs no cluster.

my $runner = "/tmp/arkime-decode-$$.js";

END { unlink $runner if $runner; }

open(my $rf, '>', $runner) or die "can't write $runner";
print $rf <<'JS';
const path = require('path');
const { Worker } = require('worker_threads');
const decodePath = path.resolve(process.argv[2], 'decode.js');

// Run in a worker so a decoder that spins can be killed
const workerSrc = `
const { parentPort, workerData } = require('worker_threads');
const decode = require(workerData.decodePath);
const options = { order: ['ITEM-HTTP'], 'ITEM-HTTP': { order: [] } };
const items = workerData.items.map((s, i) => ({ client: i % 2, ts: i + 1, data: Buffer.from(s, 'latin1') }));
const pipes = decode.createPipeline(options, options.order, new decode.Pcap2ItemStream(options, items));
const last = pipes[pipes.length - 1];
let count = 0;
last.on('data', () => { count++; });
last.on('end', () => parentPort.postMessage('done ' + count));
`;

const w = new Worker(workerSrc, { eval: true, workerData: { decodePath, items: JSON.parse(process.argv[3]) } });
const timer = setTimeout(() => { console.log('hung'); process.exit(0); }, 5000);
w.on('message', (m) => { clearTimeout(timer); console.log(m); process.exit(0); });
w.on('error', (e) => { clearTimeout(timer); console.log('error ' + e.message); process.exit(0); });
JS
close($rf);

sub decode {
    my ($req) = @_;
    my $items = to_json([$req, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"]);
    $items =~ s/'/'\\''/g;
    my $out = `node $runner ../viewer '$items' 2>&1`;
    chomp $out;
    return $out;
}

# Valid lengths still decode
like(decode("POST /x HTTP/1.1\r\nContent-Length: 1\r\n\r\nA"), qr/^done \d+$/, "Content-Length: 1");
like(decode("POST /x HTTP/1.1\r\nContent-Length: 0\r\n\r\nGET /y HTTP/1.1\r\n\r\n"), qr/^done \d+$/, "Content-Length: 0 then pipelined request");
like(decode("POST /x HTTP/1.1\r\nContent-Length:  2 \r\n\r\nAB"), qr/^done \d+$/, "Content-Length with spaces");

# Bad lengths must not hang the decoder
like(decode("POST /x HTTP/1.1\r\nContent-Length: x\r\n\r\nA"), qr/^done \d+$/, "Content-Length: x");
like(decode("POST /x HTTP/1.1\r\nContent-Length: -5\r\n\r\nABCDEFGH"), qr/^done \d+$/, "Content-Length: -5");
like(decode("POST /x HTTP/1.1\r\nContent-Length: 1e3\r\n\r\nA"), qr/^done \d+$/, "Content-Length: 1e3");
like(decode("POST /x HTTP/1.1\r\nContent-Length: 99999999999999999999\r\n\r\nA"), qr/^done \d+$/, "Content-Length too big");
like(decode("POST /x HTTP/1.1\r\nContent-Length:\r\n\r\nA"), qr/^done \d+$/, "Content-Length empty");

# Bad chunk sizes are a decode error, a negative one used to move backwards and spin
like(decode("POST /x HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nABC\r\n0\r\n\r\n"), qr/done \d+$/, "chunk size 3");
like(decode("POST /x HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\nABC\r\n0\r\n\r\n"), qr/done \d+$/, "chunk size zz");
like(decode("POST /x HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n-5\r\nABCDEFGHIJ\r\n0\r\n\r\n"), qr/done \d+$/, "chunk size -5");
like(decode("POST /x HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n" . ("f" x 40) . "\r\nABC\r\n0\r\n\r\n"), qr/done \d+$/, "chunk size too big");
