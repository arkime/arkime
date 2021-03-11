# add random fake data
./fakedatas.pl --http 4321
# add specific fake data before spike
./fakedatas.pl --http 23 --sticky-dst 10.180.156.141 --sticky-user elyse --sticky-path im/in/crisis.php --sticky-last 1612937820000
./fakedatas.pl --http 34 --sticky-dst 10.180.156.141 --sticky-user elyse --sticky-path im/in/crisis.php --sticky-last 1612937880000
./fakedatas.pl --http 45 --sticky-dst 10.180.156.141 --sticky-user elyse --sticky-path im/in/crisis.php --sticky-last 1612937940000
# add specific fake data at spike
./fakedatas.pl --http 123 --sticky-dst 10.180.156.141 --sticky-user elyse --sticky-path im/in/crisis.php --sticky-last 1612938000000
# add specific fake data after spike
./fakedatas.pl --http 45 --sticky-dst 10.180.156.141 --sticky-user elyse --sticky-path im/in/crisis.php --sticky-last 1612938060000
./fakedatas.pl --http 34 --sticky-dst 10.180.156.141 --sticky-user elyse --sticky-path im/in/crisis.php --sticky-last 1612938120000
./fakedatas.pl --http 23 --sticky-dst 10.180.156.141 --sticky-user elyse --sticky-path im/in/crisis.php --sticky-last 1612938180000

