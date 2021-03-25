#!/usr/bin/perl -I.

use strict;
use JSON;
use Data::Dumper;
use MolochTest;
use Cwd;

$main::userAgent = LWP::UserAgent->new(timeout => 20);

my $ELASTICSEARCH = $ENV{ELASTICSEARCH} = "http://127.0.0.1:9200";
my $DEBUG = 0;
my $STICKYSRC;
my $STICKYDST;
my $STICKYUSER;
my $STICKYPATH;
my $STICKYLAST;
my $TAG = "fakeit";


$ENV{'PERL5LIB'} = getcwd();
$ENV{'TZ'} = 'US/Eastern';

################################################################################
my @USERS = qw( andy elyse katie corey matt mark kyle gretchen aileen brandon abdullah paul sadiah sean art);
sub user() { return $USERS[ rand @USERS ]; }

################################################################################
my @USERAGENTS = ("Chrome/72.0.3626.109 Intel Mac OS X 10_14_3",
"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322)",
"Mozilla/4.0 (compatible;+MSIE+8.0;+Windows+NT+5.1)",
"Mozilla/5.0 (Linux; Android 6.0.1; Nexus 5 Build/M4B30Z) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/44.0.2403.133 Mobile Safari/537.36",
"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_8_5) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/31.0.1650.63 Safari/537.36",
"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/42.0.2311.135 Safari/537.36 Edge/12.10240",
"Mozilla/5.0 (Windows NT 5.1; rv:25.0) Gecko/20100101 Firefox/25.0",
"Mozilla/5.0 (Windows NT 5.1; rv:32.0) Gecko/20100101 Firefox/32.0",
"Mozilla/5.0 (Windows NT 5.2) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/33.0.1750.154 Safari/537.36",
"Mozilla/5.0 (Windows NT 6.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/36.0.1985.125 Safari/537.36",
"Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/35.0.1916.153 Safari/537.36",
"Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/44.0.2403.125 Safari/537.36",
"Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.103 Safari/537.36",
"Mozilla/5.0 (Windows NT 6.1; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.87 Safari/537.36",
"Mozilla/5.0 (Windows NT 6.3; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/63.0.3239.108 Safari/537.36",
"Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US; rv:1.9.2.3) Gecko/20100401 Firefox/3.6.3",
"Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 6.2)",
"Mutt/1.5.20 (2009-12-10)",
"Nokia-eheprb/1",
"Wget/1.15 (linux-gnu)",
"canary Chrome/44.0.2375.0",
"canary Chrome/53.0.2782.0 Windows NT 6.1; Win64; x64",
"curl/7.19.7 (x86_64-redhat-linux-gnu) libcurl/7.19.7 NSS/3.21 Basic ECC zlib/1.2.3 libidn/1.18 libssh2/1.4.2",
"curl/7.24.0 (x86_64-apple-darwin12.0) libcurl/7.24.0 OpenSSL/0.9.8y zlib/1.2.5",
"curl/7.64.1");
sub useragent() { return $USERAGENTS[ rand @USERAGENTS ]; }

################################################################################
# https://www.ef.edu/english-resources/english-vocabulary/top-3000-words/
my @WORDS = qw(
a abandon ability able about above abroad absence absolute absolutely absorb abuse academic accept access
accident accompany accomplish according account accurate accuse achieve achievement acid acknowledge acquire across act
action active activist activity actor actress actual actually ad adapt add addition additional address adequate adjust
adjustment administration administrator admire admission admit adolescent adopt adult advance advanced advantage
adventure advertising advice advise adviser advocate affair affect afford afraid african african-american after
afternoon again against age agency agenda agent aggressive ago agree agreement agricultural ah ahead aid aide aim air
aircraft airline airport album alcohol alive all alliance allow ally almost alone along already also alter alternative
although always am amazing american among amount analysis analyst analyze ancient and anger angle angry animal
anniversary announce annual another answer anticipate anxiety any anybody anymore anyone anything anyway anywhere apart
apartment apparent apparently appeal appear appearance apple application apply appoint appointment appreciate approach
appropriate approval approve approximately arab architect area argue argument arise arm armed army around arrange
arrangement arrest arrival arrive art article artist artistic as asian aside ask asleep aspect assault assert assess
assessment asset assign assignment assist assistance assistant associate association assume assumption assure at
athlete athletic atmosphere attach attack attempt attend attention attitude attorney attract attractive attribute
audience author authority auto available average avoid award aware awareness away awful baby back background bad badly
bag bake balance ball ban band bank bar barely barrel barrier base baseball basic basically basis basket basketball
bathroom battery battle be beach bean bear beat beautiful beauty because become bed bedroom beer before begin beginning
behavior behind being belief believe bell belong below belt bench bend beneath benefit beside besides best bet better
between beyond big bike bill billion bind biological bird birth birthday bit bite black blade blame blanket blind
block blood blow blue board boat body bomb bombing bond bone book boom boot border born borrow boss both bother bottle
bottom boundary bowl box boy boyfriend brain branch brand bread break breakfast breast breath breathe brick bridge
brief briefly bright brilliant bring british broad broken brother brown brush buck budget build building bullet bunch
burden burn bury bus business busy but butter button buy buyer by cabin cabinet cable cake calculate call camera camp
campaign campus can canadian cancer candidate cap capability capable capacity capital captain capture car carbon card
care career careful carefully carrier carry case cash cast cat catch category catholic cause ceiling celebrate
celebration celebrity cell center central century ceo ceremony certain certainly chain chair chairman challenge chamber
champion championship chance change changing channel chapter character characteristic characterize charge charity chart
chase cheap check cheek cheese chef chemical chest chicken chief child childhood chinese chip chocolate choice
cholesterol choose christmas church cigarette circle circumstance cite citizen city civil civilian claim
class classic classroom clean clear clearly client climate climb clinic clinical clock close closely closer clothes
clothing cloud club clue cluster coach coal coalition coast coat code coffee cognitive cold collapse colleague collect
collection collective college colonial color column combination combine come comedy comfort comfortable command
commander comment commercial commission commit commitment committee common communicate communication community company
compare comparison compete competition competitive competitor complain complaint complete completely complex
complicated component compose composition comprehensive computer concentrate concentration concept concern concerned
concert conclude conclusion concrete condition conduct conference confidence confident confirm conflict confront
confusion congress congressional connect connection consciousness consensus consequence conservative consider
considerable consideration consist consistent constant constantly constitute constitutional construct construction
consultant consume consumer consumption contact contain container contemporary content contest context continue
continued contract contrast contribute contribution control controversial controversy convention conventional
conversation convert conviction convince cook cookie cooking cool cooperation cop cope copy core corn corner corporate
corporation correct correspondent cost cotton couch could council counselor count counter country county couple courage
course court cousin cover coverage cow crack craft crash crazy cream create creation creative creature credit crew
crime criminal crisis criteria critic critical criticism criticize crop cross crowd crucial cry cultural culture cup
curious current currently curriculum custom customer cut cycle dad daily damage dance danger dangerous dare dark
darkness data date daughter day dead deal dealer dear death debate debt decade decide decision deck declare decline
decrease deep deeply deer defeat defend defendant defense defensive deficit define definitely definition degree delay
deliver delivery demand democracy democrat democratic demonstrate demonstration deny department depend dependent
depending depict depression depth deputy derive describe description desert deserve design designer desire desk
desperate despite destroy destruction detail detailed detect determine develop developing development device devote
dialogue die diet differ difference different differently difficult difficulty dig digital dimension dining dinner
direct direction directly director dirt dirty disability disagree disappear disaster discipline discourse discover
discovery discrimination discuss discussion disease dish dismiss disorder display dispute distance distant distinct
distinction distinguish distribute distribution district diverse diversity divide division divorce dna do doctor
document dog domestic dominant dominate door double doubt down downtown dozen draft drag drama dramatic dramatically
draw drawing dream dress drink drive driver drop drug dry due during dust duty each eager ear early earn earnings earth
ease easily east eastern easy eat economic economics economist economy edge edition editor educate education
educational educator effect effective effectively efficiency efficient effort egg eight either elderly elect election
electric electricity electronic element elementary eliminate elite else elsewhere e-mail embrace emerge emergency
emission emotion emotional emphasis emphasize employ employee employer employment empty enable encounter encourage end
enemy energy enforcement engage engine engineer engineering english enhance enjoy enormous enough ensure enter
enterprise entertainment entire entirely entrance entry environment environmental episode equal equally equipment era
error escape especially essay essential essentially establish establishment estate estimate etc ethics ethnic european
evaluate evaluation even evening event eventually ever every everybody everyday everyone everything everywhere evidence
evolution evolve exact exactly examination examine example exceed excellent except exception exchange exciting
executive exercise exhibit exhibition exist existence existing expand expansion expect expectation expense expensive
experience experiment expert explain explanation explode explore explosion expose exposure express expression extend
extension extensive extent external extra extraordinary extreme extremely eye fabric face facility fact factor factory
faculty fade fail failure fair fairly faith fall false familiar family famous fan fantasy far farm farmer fashion fast
fat fate father fault favor favorite fear feature federal fee feed feel feeling fellow female fence few fewer fiber
fiction field fifteen fifth fifty fight fighter fighting figure file fill film final finally finance financial find
finding fine finger finish fire firm first fish fishing fit fitness five fix flag flame flat flavor flee flesh flight
float floor flow flower fly focus folk follow following food foot football for force foreign forest forever forget form
formal formation former formula forth fortune forward found foundation founder four fourth frame framework free freedom
freeze french frequency frequent frequently fresh friend friendly friendship from front fruit frustration fuel full
fully fun function fund fundamental funding funeral funny furniture furthermore future gain galaxy gallery game gang
gap garage garden garlic gas gate gather gay gaze gear gender gene general generally generate generation genetic
gentleman gently german gesture get ghost giant gift gifted girl girlfriend give given glad glance glass global glove
go goal god gold golden golf good government governor grab grade gradually graduate grain grand grandfather grandmother
grant grass grave gray great greatest green grocery ground group grow growing growth guarantee guard guess guest guide
guideline guilty gun guy habit habitat hair half hall hand handful handle hang happen happy hard hardly hat hate have
he head headline headquarters health healthy hear hearing heart heat heaven heavily heavy heel height helicopter hell
hello help helpful her here heritage hero herself hey hi hide high highlight highly highway hill him himself hip hire
his historian historic historical history hit hold hole holiday holy home homeless honest honey honor hope horizon
horror horse hospital host hot hotel hour house household housing how however huge human humor hundred hungry hunter
hunting hurt husband hypothesis i ice idea ideal identification identify identity ie if ignore ill illegal illness
illustrate image imagination imagine immediate immediately immigrant immigration impact implement implication imply
importance important impose impossible impress impression impressive improve improvement in incentive incident include
including income incorporate increase increased increasing increasingly incredible indeed independence independent
index indian indicate indication individual industrial industry infant infection inflation influence inform information
ingredient initial initially initiative injury inner innocent inquiry inside insight insist inspire install instance
instead institution institutional instruction instructor instrument insurance intellectual intelligence intend intense
intensity intention interaction interest interested interesting internal international internet interpret
interpretation intervention interview into introduce introduction invasion invest investigate investigation
investigator investment investor invite involve involved involvement iraqi irish iron islamic island israeli issue it
italian item its itself jacket jail japanese jet jewish job join joint joke journal journalist journey joy judge
judgment juice jump junior jury just justice justify keep key kick kid kill killer killing kind king kiss kitchen knee
knife knock know knowledge lab label labor laboratory lack lady lake land landscape language lap large largely last
late later latin latter laugh launch law lawn lawsuit lawyer lay layer lead leader leadership leading leaf league lean
learn learning least leather leave left leg legacy legal legend legislation legitimate lemon length less lesson let
letter level liberal library license lie life lifestyle lifetime lift light like likely limit limitation limited line
link lip list listen literally literary literature little live living load loan local locate location lock long
long-term look loose lose loss lost lot lots loud love lovely lover low lower luck lucky lunch lung machine mad
magazine mail main mainly maintain maintenance major majority make maker makeup male mall man manage management manager
manner manufacturer manufacturing many map margin mark market marketing marriage married marry mask mass massive master
match material math matter may maybe mayor me meal mean meaning meanwhile measure measurement meat mechanism media
medical medication medicine medium meet meeting member membership memory mental mention menu mere merely mess message
metal meter method mexican middle might military milk million mind mine minister minor minority minute miracle mirror
miss missile mission mistake mix mixture mm-hmm mode model moderate modern modest mom moment money monitor month mood
moon moral more moreover morning mortgage most mostly mother motion motivation motor mount mountain mouse mouth move
movement movie mr mrs ms much multiple murder muscle museum music musical musician muslim must mutual my myself mystery
myth naked name narrative narrow nation national native natural naturally nature near nearby nearly necessarily
necessary neck need negative negotiate negotiation neighbor neighborhood neither nerve nervous net network never
nevertheless new newly news newspaper next nice night nine no nobody nod noise nomination none nonetheless nor normal
normally north northern nose not note nothing notice notion novel now nowhere n't nuclear number numerous nurse nut
object objective obligation observation observe observer obtain obvious obviously occasion occasionally occupation
occupy occur ocean odd odds of off offense offensive offer office officer official often oh oil ok okay old olympic on
once one ongoing onion online only onto open opening operate operating operation operator opinion opponent opportunity
oppose opposite opposition option or orange order ordinary organic organization organize orientation origin original
originally other others otherwise ought our ourselves out outcome outside oven over overall overcome overlook owe own
owner pace pack package page pain painful paint painter painting pair pale palestinian palm pan panel pant paper parent
park parking part participant participate participation particular particularly partly partner partnership party pass
passage passenger passion past patch path patient pattern pause pay payment pc peace peak peer penalty people pepper
per perceive percentage perception perfect perfectly perform performance perhaps period permanent permission permit
person personal personality personally personnel perspective persuade pet phase phenomenon philosophy phone photo
photograph photographer phrase physical physically physician piano pick picture pie piece pile pilot pine pink pipe
pitch place plan plane planet planning plant plastic plate platform play player please pleasure plenty plot plus pm
pocket poem poet poetry point pole police policy political politically politician politics poll pollution pool poor pop
popular population porch port portion portrait portray pose position positive possess possibility possible possibly
post pot potato potential potentially pound pour poverty powder power powerful practical practice pray prayer precisely
predict prefer preference pregnancy pregnant preparation prepare prescription presence present presentation preserve
president presidential press pressure pretend pretty prevent previous previously price pride priest primarily primary
prime principal principle print prior priority prison prisoner privacy private probably problem procedure proceed
process produce producer product production profession professional professor profile profit program progress project
prominent promise promote prompt proof proper properly property proportion proposal propose proposed prosecutor
prospect protect protection protein protest proud prove provide provider province provision psychological psychologist
psychology public publication publicly publish publisher pull punishment purchase pure purpose pursue push put qualify
quality quarter quarterback question quick quickly quiet quietly quit quite quote race racial radical radio rail rain
raise range rank rapid rapidly rare rarely rate rather rating ratio raw reach react reaction read reader reading ready
real reality realize really reason reasonable recall receive recent recently recipe recognition recognize recommend
recommendation record recording recover recovery recruit red reduce reduction refer reference reflect reflection reform
refugee refuse regard regarding regardless regime region regional register regular regularly regulate regulation
reinforce reject relate relation relationship relative relatively relax release relevant relief religion religious rely
remain remaining remarkable remember remind remote remove repeat repeatedly replace reply report reporter represent
representation representative republican reputation request require requirement research researcher resemble
reservation resident resist resistance resolution resolve resort resource respect respond respondent response
responsibility responsible rest restaurant restore restriction result retain retire retirement return reveal revenue
review revolution rhythm rice rich rid ride rifle right ring rise risk river road rock role roll romantic roof room
root rope rose rough roughly round route routine row rub rule run running rural rush russian sacred sad safe safety
sake salad salary sale sales salt same sample sanction sand satellite satisfaction satisfy sauce save saving say scale
scandal scared scenario scene schedule scheme scholar scholarship school science scientific scientist scope score
scream screen script sea search season seat second secret secretary section sector secure security see seed seek seem
segment seize select selection self sell senate senator send senior sense sensitive sentence separate sequence series
serious seriously serve service session set setting settle settlement seven several severe sex sexual shade shadow
shake shall shape share sharp she sheet shelf shell shelter shift shine ship shirt shock shoe shoot shooting shop
shopping shore short shortly shot should shoulder shout show shower shrug shut sick side sigh sight sign signal
significance significant significantly silence silent silver similar similarly simple simply sin since sing singer
single sink sir sister sit site situation six size ski skill skin sky slave sleep slice slide slight slightly slip slow
slowly small smart smell smile smoke smooth snap snow so so-called soccer social society soft software soil solar
soldier solid solution solve some somebody somehow someone something sometimes somewhat somewhere son song soon
sophisticated sorry sort soul sound soup source south southern soviet space spanish speak speaker special specialist
species specific specifically speech speed spend spending spin spirit spiritual split spokesman sport spot spread
spring square squeeze stability stable staff stage stair stake stand standard standing star stare start state statement
station statistics status stay steady steal steel step stick still stir stock stomach stone stop storage store storm
story straight strange stranger strategic strategy stream street strength strengthen stress stretch strike string strip
stroke strong strongly structure struggle student studio study stuff stupid style subject submit subsequent substance
substantial succeed success successful successfully such sudden suddenly sue suffer sufficient sugar suggest suggestion
suicide suit summer summit sun super supply support supporter suppose supposed supreme sure surely surface surgery
surprise surprised surprising surprisingly surround survey survival survive survivor suspect sustain swear sweep sweet
swim swing switch symbol symptom system table tablespoon tactic tail take tale talent talk tall tank tap tape target
task taste tax taxpayer tea teach teacher teaching team tear teaspoon technical technique technology teen teenager
telephone telescope television tell temperature temporary ten tend tendency tennis tension tent term terms terrible
territory terror terrorism terrorist test testify testimony testing text than thank thanks that the theater their them
theme themselves then theory therapy there therefore these they thick thin thing think thinking third thirty this those
though thought thousand threat threaten three throat through throughout throw thus ticket tie tight time tiny tip tire
tired tissue title to tobacco today toe together tomato tomorrow tone tongue tonight too tool tooth top topic toss
total totally touch tough tour tourist tournament toward towards tower town toy trace track trade tradition traditional
traffic tragedy trail train training transfer transform transformation transition translate transportation travel treat
treatment treaty tree tremendous trend trial tribe trick trip troop trouble truck true truly trust truth try tube
tunnel turn tv twelve twenty twice twin two type typical typically ugly ultimate ultimately unable uncle under undergo
understand understanding unfortunately uniform union unique unit united universal universe university unknown unless
unlike unlikely until unusual up upon upper urban urge us use used useful user usual usually utility vacation valley
valuable value variable variation variety various vary vast vegetable vehicle venture version versus very vessel
veteran via victim victory video view viewer village violate violation violence violent virtually virtue virus visible
vision visit visitor visual vital voice volume volunteer vote voter vs vulnerable wage wait wake walk wall wander want
war warm warn warning wash waste watch water wave way we weak wealth wealthy weapon wear weather wedding week weekend
weekly weigh weight welcome welfare well west western wet what whatever wheel when whenever where whereas whether which
while whisper white who whole whom whose why wide widely widespread wife wild will willing win wind window wine wing
winner winter wipe wire wisdom wise wish with withdraw within without witness woman wonder wonderful wood wooden word
work worker working works workshop world worried worry worth would wound wrap write writer writing wrong yard yeah year
yell yellow yes yesterday yet yield you young your yours yourself youth zone);

sub word() { return $WORDS[ rand @WORDS ]; }

################################################################################
my @IPS = qw(10.0.0.1 10.0.0.10 10.0.0.11 10.0.0.12 10.0.0.16 10.0.0.17 10.0.0.2 10.0.0.3 10.0.0.32 10.0.0.33 10.0.0.4 10.0.0.5 10.0.0.6 10.0.0.7 10.0.0.8 10.0.0.9 10.0.13.120 10.0.2.15 10.1.2.1 10.1.2.2 10.10.0.2 10.10.0.3 10.10.10.1 10.10.10.10 10.10.10.11 10.10.10.12 10.10.10.13 10.10.10.14 10.10.10.15 10.10.10.16 10.10.10.17 10.10.10.18 10.10.10.19 10.10.10.2 10.10.10.20 10.10.10.30 10.10.30.26 10.11.11.11 10.11.12.13 10.12.12.12 10.13.13.13 10.150.10.150 10.156.206.202 10.172.10.16 10.172.10.172 10.176.171.11 10.176.176.11 10.176.192.13 10.178.8.71 10.180.121.109 10.180.121.151 10.180.152.137 10.180.156.141 10.180.156.185 10.180.156.249 10.2.95.39 10.23.46.37 10.3.4.5 10.34.0.1 10.44.100.22 10.5.4.3 10.64.11.49 10.89.85.15 10.9.8.7 104.16.125.34 104.89.119.175 118.215.80.242 127.0.0.1 129.170.17.4 129.21.171.72 13.115.50.210 13.12.11.10 139.162.123.134 14.17.32.211 155.230.24.155 172.130.128.76 172.16.0.1 172.16.0.2 172.16.0.3 172.16.0.4 172.16.44.3 172.17.96.143 172.17.96.77 172.202.246.57 172.28.2.3 172.5.5.113 173.194.68.26 18.26.4.105 188.40.206.23 190.0.0.1 190.0.0.12 190.0.0.13 190.0.0.14 190.0.0.15 190.0.0.2 190.0.0.22 190.0.0.23 190.0.0.24 190.0.0.25 190.0.0.3 190.0.0.4 190.0.0.5 192.168.0.1 192.168.0.10 192.168.1.111 192.168.1.3 192.168.100.1 192.168.100.2 192.168.114.1 192.168.114.129 192.168.168.1 192.168.170.20 192.168.170.56 192.168.170.8 192.168.177.160 192.168.178.20 192.168.235.1 192.168.235.136 192.168.25.150 192.168.3.129 192.168.40.178 192.168.56.11 192.168.56.12 192.168.57.14 192.168.65.1 192.168.65.3 192.168.8.97 192.30.252.130 192.30.252.131 193.0.14.129 193.242.192.43 20.0.1.100 202.11.40.158 203.255.252.194 204.62.14.153 205.188.186.167 216.58.194.195 216.58.208.195 217.13.4.24 217.70.190.232 222.96.156.151 224.0.0.1 224.0.0.13 224.0.0.22 224.0.0.5 23.0.0.2 23.0.0.3 255.255.255.255 31.13.74.1 35.174.150.168 38.229.70.20 52.43.228.156 54.226.182.138 64.12.168.40 64.12.189.217 64.12.21.3 64.15.116.182 64.236.55.17 64.236.55.18 64.236.64.225 64.236.64.226 66.59.111.182 66.59.111.190 68.178.213.61 74.125.228.103 74.125.228.226 74.125.228.238 74.125.228.37 74.125.228.39 74.125.24.100 74.125.24.149 74.125.24.95 74.217.87.13 8.8.8.8);
sub ip() { return $IPS[ rand @IPS ]; }

################################################################################
my @ASNS = ("AS10052 Kyungpook National University", "AS10755 Dartmouth College", "AS10913 Internap Corporation", "AS11590 Cumberland Technologies International", "AS13335 Cloudflare, Inc.", "AS13489 EPM Telecomunicaciones S.A. E.S.P.", "AS14618 Amazon.com, Inc.", "AS15133 MCI Communications Services, Inc. d/b/a Verizon Business", "AS15169 Google LLC", "AS15733 Zumtobel Group AG", "AS16150 Availo Networks AB", "AS16509 Amazon.com, Inc.", "AS16625 Akamai Technologies, Inc.", "AS203476 GANDI SAS", "AS20940 Akamai International B.V.", "AS23028 Team Cymru Inc.", "AS26496 GoDaddy.com, LLC", "AS2830 Verizon Nederland B.V.", "AS3 Massachusetts Institute of Technology", "AS32934 Facebook, Inc.", "AS36459 GitHub, Inc.", "AS43515 Google Ireland Limited", "AS4385 Rochester Institute of Technology", "AS4589 EASYNET Easynet Global Services", "AS4657 StarHub Ltd", "AS46636 NatCoWeb Corp.", "AS4766 Korea Telecom", "AS4816 China Telecom (Group)", "AS7018 AT&T Services, Inc.", "AS7233 Yahoo", "AS9270 National Infomation Society Agency");
sub asn() { return $ASNS[ rand @ASNS ]; }

################################################################################
my @HTMLEXTENSIONS = ("", ".html", ".jpg", ".gif", ".php", ".cgi");
sub htmlextension() { return $HTMLEXTENSIONS[ rand @HTMLEXTENSIONS ]; }

################################################################################
my @GEOS = qw(AT CA CN CO DE FR GB IE JP KR NL NO RU SE SG US);
sub geo() { return $GEOS[ rand @GEOS ]; }

################################################################################
sub num
{
    my ($min, $max) = @_;
    return $min + int(rand($max-$min));
}
################################################################################
sub generateHTTP()
{
    my $uriCnt = num(1, 10);

    my $host = "${\word()}.com";

    my @paths;
    my @uris;
    for (my $i = 0; $i < $uriCnt; $i++) {
        my $path = "${\word()}/${\word()}${\htmlextension()}";
        if ($i == 0 && $STICKYPATH) {
            $path = $STICKYPATH;
        }
        push(@paths, qq("$path"));
        push(@uris, qq("${host}/${path}"));
    }

    my $dstIp = $STICKYDST || ip();
    my $srcIp = $STICKYSRC || ip();
    my $user = $STICKYUSER || user();

    my $lastPacket = $STICKYLAST || (time() - num(100, 100000)) * 1000;
    my $firstPacket =  $lastPacket - (num(10, 1000) * 1000);

    my $srcPackets = num(1, 8000);
    my $dstPackets = num(1, 8000);
    my $srcBytes   = $srcPackets * num(10,1000);
    my $dstBytes   = $dstPackets * num(10,1000);
    my $srcDataBytes  = int($srcBytes * 0.90);
    my $dstDataBytes  = int($dstBytes * 0.90);

    my $json = qq(
{
"communityId" : "1:FjsyR6OWh6MqgDDUvtPR73EKFnQ=",
"dstASN" : "${\asn()}",
"dstBytes" : ${dstBytes},
"dstDataBytes" : ${dstDataBytes},
"dstGEO" : "${\geo()}",
"dstIp" : "${dstIp}",
"dstMac" : [
   "00:c0:ca:30:eb:0c"
],
"dstMacCnt" : 1,
"dstOui" : [
   "Alfa, Inc."
],
"dstOuiCnt" : 1,
"dstPackets" : ${dstPackets},
"dstPayload8" : "485454502f312e30",
"dstPort" : 80,
"fileId" : [],
"firstPacket" : ${firstPacket},
"http" : {
   "bodyMagic" : [
      "text/javascript"
   ],
   "bodyMagicCnt" : 1,
   "clientVersion" : [
      "1.1"
   ],
   "clientVersionCnt" : 1,
   "cookieKey" : [
      "trafic_ranking"
   ],
   "cookieKeyCnt" : 1,
   "cookieValue" : [
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
   ],
   "cookieValueCnt" : 1,
   "host" : [
      "${host}"
   ],
   "hostCnt" : 1,
   "md5" : [
      "9fb54a2726ca3cf54a82804d0e66d08a"
   ],
   "md5Cnt" : 1,
   "method" : [
      "GET"
   ],
   "methodCnt" : 1,
   "path" : [
      ${\join(',', @paths)}
   ],
   "pathCnt" : ${uriCnt},
   "request-referer" : [
      "http://${host}/${\word()}/${\word()}.html"
   ],
   "request-refererCnt" : 1,
   "requestHeader" : [
      "accept",
      "accept-charset",
      "accept-encoding",
      "accept-language",
      "connection",
      "cookie",
      "host",
      "keep-alive",
      "referer",
      "user-agent"
   ],
   "requestHeaderCnt" : 10,
   "requestHeaderField" : [
      "accept",
      "accept-charset",
      "accept-encoding",
      "accept-language",
      "connection",
      "cookie",
      "keep-alive"
   ],
   "requestHeaderValue" : [
      "*/*",
      "115",
      "en-us,en;q=0.5",
      "gzip,deflate",
      "iso-8859-1,utf-8;q=0.7,*;q=0.7",
      "keep-alive",
      "trafic_ranking=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
   ],
   "requestHeaderValueCnt" : 7,
   "responseHeader" : [
      "cache-control",
      "connection",
      "content-type",
      "date",
      "expires",
      "last-modified",
      "p3p",
      "pragma",
      "server",
      "set-cookie"
   ],
   "responseHeaderCnt" : 10,
   "responseHeaderField" : [
      "cache-control",
      "connection",
      "content-type",
      "date",
      "expires",
      "last-modified",
      "p3p",
      "pragma",
      "server",
      "set-cookie"
   ],
   "responseHeaderValue" : [
      "apache",
      "application/x-javascript",
      "close",
      "mon, 10 may 2010 08:31:02 gmt",
      "mon, 10 may 2010 08:31:02 gmt",
      "no-cache",
      "no-store, no-cache, must-revalidate, post-check=0, pre-check=0",
      "policyref=\\"/w3c/p3p.xml\\", cp=\\"all ind dsp cor adm cono cur ivao ivdo psa psd tai telo our samo cnt com int nav onl phy pre pur uni\\"",
      "thu, 11 jan 1973 16:00:00 gmt",
      "trafic_ranking=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx; expires=sun, 11-jan-2037 14:00:00 gmt; path=/; domain=.xxxxxx.xx"
   ],
   "responseHeaderValueCnt" : 10,
   "serverVersion" : [
      "1.0"
   ],
   "serverVersionCnt" : 1,
   "sha256" : [
      "be86ba67700e1e23003ee9899119995d2babebbf6d753aafdd4a63017e6cb7b7"
   ],
   "sha256Cnt" : 1,
   "statuscode" : [
      200
   ],
   "statuscodeCnt" : 1,
   "uri" : [
     ${\join(',', @uris)}
   ],
   "uriCnt" : ${uriCnt},
   "user": "${user}",
   "useragent" : [
      "${\useragent()}"
   ],
   "useragentCnt" : 1
},
"initRTT" : 5,
"ipProtocol" : 6,
"lastPacket" : ${lastPacket},
"length" : 124,
"node" : "test",
"packetLen" : [
],
"packetPos" : [
],
"protocol" : [
   "http",
   "tcp"
],
"protocolCnt" : 2,
"segmentCnt" : 1,
"srcASN" : "${\asn()}",
"srcBytes" : ${srcBytes},
"srcDataBytes" : ${srcDataBytes},
"srcGEO" : "${\geo()}",
"srcIp" : "${srcIp}",
"srcMac" : [
   "00:16:44:a0:a0:7e"
],
"srcMacCnt" : 1,
"srcOui" : [
   "LITE-ON Technology Corp."
],
"srcOuiCnt" : 1,
"srcPackets" : ${srcPackets},
"srcPayload8" : "474554202f6a732f",
"srcPort" : ${\num(1,65000)},
"tags" : [
   "$TAG",
   "dstip",
   "srcip"
],
"tagsCnt" : 3,
"tcpflags" : {
   "ack" : 4,
   "dstZero" : 0,
   "fin" : 2,
   "psh" : 3,
   "rst" : 0,
   "srcZero" : 0,
   "syn" : 1,
   "syn-ack" : 1,
   "urg" : 0
},
"timestamp" : ${lastPacket},
"totBytes" : @{[$srcBytes + $dstBytes]},
"totDataBytes" : @{[$srcDataBytes + $dstDataBytes]},
"totPackets" : @{[$srcPackets + $dstPackets]}
}
);

  my @tinfo = gmtime($lastPacket/1000);
  my $index = sprintf("tests_sessions2-%02d%02d%02d", $tinfo[5]%100, $tinfo[4]+1, $tinfo[3]);

  print $json if ($DEBUG);
  my $result = esPost("/$index/_doc", $json);
  print Dumper($result) if ($DEBUG);
}

################################################################################
my $HTTP = 100;
for (my $pos;$pos <= $#ARGV; $pos++) {
    if ($ARGV[$pos] eq "--debug") {
        $DEBUG = 1;
    } elsif ($ARGV[$pos] eq "--http") {
        $pos++;
        $HTTP = int($ARGV[$pos]);
    } elsif ($ARGV[$pos] eq "--sticky-src") {
        if ($pos < $#ARGV && @ARGV[$pos + 1] !~ /^--/) {
            $pos++;
            $STICKYSRC = $ARGV[$pos];
        } else {
            $STICKYSRC = ip();
        }
    } elsif ($ARGV[$pos] eq "--sticky-dst") {
        if ($pos < $#ARGV && @ARGV[$pos + 1] !~ /^--/) {
            $pos++;
            $STICKYDST = $ARGV[$pos];
        } else {
            $STICKYDST = ip();
        }
    } elsif ($ARGV[$pos] eq "--sticky-user") {
        if ($pos < $#ARGV && @ARGV[$pos + 1] !~ /^--/) {
            $pos++;
            $STICKYUSER = $ARGV[$pos];
        } else {
            $STICKYUSER = user();
        }
    } elsif ($ARGV[$pos] eq "--sticky-path") {
        if ($pos < $#ARGV && @ARGV[$pos + 1] !~ /^--/) {
            $pos++;
            $STICKYPATH = $ARGV[$pos];
        } else {
            $STICKYPATH = "${\word()}/${\word()}${\htmlextension()}";
        }
    } elsif ($ARGV[$pos] eq "--sticky-last") {
        if ($pos < $#ARGV && @ARGV[$pos + 1] !~ /^--/) {
            $pos++;
            $STICKYLAST = int($ARGV[$pos]);
        } else {
            $STICKYLAST = (time() - num(100, 100000)) * 1000;
        }
    } elsif ($ARGV[$pos] eq "--tag") {
        $pos++;
        $TAG = $ARGV[$pos];
    } else {
        print "--debug                - turn on debugging100\n";
        print "--http <number>        - number of http records to create, default 100\n";
        print "--sticky-src [<ip>]    - All src ips will be the same, which can be optionally provided\n";
        print "--sticky-dst [<ip>]    - All dst ips will be the same, which can be optionally provided\n";
        print "--sticky-user [<user>] - All users will be the same, which can be optionally provided\n";
        print "--sticky-path [<path>] - The first path for each suession will be the same, which can be optionally provided\n";
        print "--sticky-last [<ms>]   - All items will have the same lastPacket time, which can be optionally provided\n";
        print "--tag <tag>            - All session will have this tag, default: $TAG\n";
        exit(0);
    }
}

print "Creating $HTTP http records\n";
for (my $i = 0; $i < $HTTP; $i++) {
    generateHTTP();
}

################################################################################
esGet("/_flush");
esGet("/_refresh");
esGet("/_flush");
esGet("/_refresh");

