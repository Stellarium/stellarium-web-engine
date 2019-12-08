// Auto generated from tools/makeassets.py

static const unsigned char DATA_planets_ini[25734] __attribute__((aligned(4))) =
    "[sun]\n"
    "type = Sun\n"
    "color = 1.0, 0.98, 0.97\n"
    "radius = 696000 km\n"
    "vmag = -26.74\n"
    "horizons_id = 10\n"
    "mass = 1.988544e+30 kg\n"
    "\n"
    "[mercury]\n"
    "type = Pla\n"
    "parent = sun\n"
    "radius = 2440 km\n"
    "vmag = -1.9\n"
    "color = 1.0, 0.964, 0.914\n"
    "shadow_brightness = 0.3\n"
    "horizons_id = 199\n"
    "orbit = plan94:1\n"
    "albedo = 0.106\n"
    "mass = 3.302e+23 kg\n"
    "\n"
    "[venus]\n"
    "type = Pla\n"
    "parent = sun\n"
    "radius = 6051.89 km\n"
    "vmag = -4.6\n"
    "color = 1., 0.96, 0.876\n"
    "shadow_brightness = 0.3\n"
    "horizons_id = 299\n"
    "orbit = plan94:2\n"
    "albedo = 0.65\n"
    "mass = 4.8685e+24 kg\n"
    "\n"
    "[earth]\n"
    "type = Pla\n"
    "parent = sun\n"
    "radius = 6378 km\n"
    "color = 0.898, 0.94, 1.\n"
    "shadow_brightness = 0.3\n"
    "horizons_id = 399\n"
    "mass = 5.972e+24 kg\n"
    "\n"
    "[mars]\n"
    "type = Pla\n"
    "parent = sun\n"
    "radius = 3394 km\n"
    "vmag = -2.91\n"
    "color = 1., 0.768, 0.504\n"
    "shadow_brightness = 0.3,\n"
    "horizons_id = 499\n"
    "orbit = plan94:4\n"
    "albedo = 0.15\n"
    "mass = 6.4185e+23 kg\n"
    "\n"
    "[jupiter]\n"
    "type = Pla\n"
    "parent = sun\n"
    "radius = 69911 km\n"
    "vmag = -2.94\n"
    "color = 1., 0.983, 0.934\n"
    "shadow_brightness = 0.3\n"
    "horizons_id = 599\n"
    "orbit = plan94:5\n"
    "albedo = 0.52\n"
    "rot_obliquity = 3.13 deg\n"
    "# Set so that the GRS position is correct.\n"
    "rot_period = 9.9289013 h\n"
    "rot_offset = 240 deg\n"
    "rot_pole_ra = 268.057 deg\n"
    "rot_pole_de = 64.495 deg\n"
    "mass = 1.89813e+27 kg\n"
    "\n"
    "[saturn]\n"
    "type = Pla\n"
    "parent = sun\n"
    "radius = 58232 km\n"
    "vmag = 0.43\n"
    "color = 1.0, 0.955, 0.858\n"
    "shadow_brightness = 0.3,\n"
    "rings_inner_radius = 74510 km\n"
    "rings_outer_radius = 140390 km\n"
    "rot_obliquity = 26.73 deg\n"
    "horizons_id = 699\n"
    "orbit = plan94:6\n"
    "albedo = 0.47\n"
    "mass = 5.68319e+26 kg\n"
    "rot_period = 10.65622 h\n"
    "rot_pole_ra = 40.589 deg\n"
    "rot_pole_de = 83.537 deg\n"
    "\n"
    "[uranus]\n"
    "type = Pla\n"
    "parent = sun\n"
    "radius = 25362 km\n"
    "vmag = 5.32\n"
    "color = 0.837, 0.959, 1.\n"
    "shadow_brightness = 0.3\n"
    "horizons_id = 799\n"
    "orbit = plan94:7\n"
    "albedo = 0.51\n"
    "mass = 8.68103e+25 kg\n"
    "\n"
    "[neptune]\n"
    "type = Pla\n"
    "parent = sun\n"
    "radius = 24624 km\n"
    "vmag = 7.78\n"
    "color = 0.44, 0.582, 1.\n"
    "shadow_brightness = 0.3\n"
    "horizons_id = 899\n"
    "orbit = plan94:8\n"
    "albedo = 0.41\n"
    "mass = 1.0241e+26 kg\n"
    "\n"
    "[pluto]\n"
    "horizons_id = 999\n"
    "radius = 1195 km\n"
    "albedo = 0.3\n"
    "orbit = horizons:2458310.500000000, A.D. 2018-Jul-11 00:00:00.0000,  2.551533849350632E-01,  4.428405097535323E+09,  1.731754769072469E+01,  1.102906097355859E+02,  1.139473143293556E+02,  2.447868046475323E+06,  4.553105469924089E-08,  4.107939171491531E+01,  6.549212880988145E+01,  5.945391988052800E+09,  7.462378878570277E+09,  7.906691430233925E+09,\n"
    "parent = sun\n"
    "type = Pla\n"
    "\n"
    "[moon]\n"
    "type = Moo\n"
    "parent = earth\n"
    "color = 1., 0.986, 0.968\n"
    "radius = 1738 km\n"
    "vmag = -12.5\n"
    "shadow_brightness = 0.1\n"
    "rot_obliquity = 1.5424 deg ; To ecliptic\n"
    "rot_period = 27.321661 d\n"
    "rot_offset = 220 deg ; Manually determined.\n"
    "horizons_id = 301\n"
    "orbit = elp\n"
    "\n"
    "[io]\n"
    "type = Moo\n"
    "parent = jupiter\n"
    "color = 1., .885, .598\n"
    "shadow_brightness = 0.1\n"
    "radius = 1821.3 km\n"
    "albedo = 0.6\n"
    "horizons_id = 501\n"
    "orbit = l12:1\n"
    "mass = 8.933e+22 kg\n"
    "\n"
    "[europa]\n"
    "type = Moo\n"
    "parent = jupiter\n"
    "color = 1., 0.968, 0.887\n"
    "shadow_brightness = 0.1\n"
    "radius = 1565 km\n"
    "albedo = 0.6\n"
    "horizons_id = 502\n"
    "orbit = l12:2\n"
    "mass = 4.797e+22 kg\n"
    "\n"
    "[ganymede]\n"
    "type = Moo\n"
    "parent = jupiter\n"
    "color = 1., 0.962, 0.871\n"
    "shadow_brightness = 0.1\n"
    "radius = 2634 km\n"
    "albedo = 0.4\n"
    "horizons_id = 503\n"
    "orbit = l12:3\n"
    "mass = 1.482e+23 kg\n"
    "\n"
    "[callisto]\n"
    "type = Moo\n"
    "parent = jupiter\n"
    "color = 1., 0.979, 0.897\n"
    "shadow_brightness = 0.1\n"
    "radius = 2403 km\n"
    "albedo = 0.2\n"
    "horizons_id = 504\n"
    "orbit = l12:4\n"
    "mass = 1.076e+23 kg\n"
    "\n"
    "[himalia]\n"
    "type = Moo\n"
    "parent = jupiter\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.565748542035758E-01,  9.652330872896690E+06,  4.876546752105060E+01,  2.719614731130625E+01,  5.022599500174405E+01,  2.458790469247338E+06,  1.665748296241506E-05,  4.178124874268134E+01,  5.562399084102634E+01,  1.144420571404976E+07,  1.323608055520283E+07,  2.161190864263720E+07,\n"
    "albedo = 0.03\n"
    "horizons_id = 506\n"
    "radius = 85 km\n"
    "\n"
    "[phobos]\n"
    "horizons_id = 401\n"
    "radius = 13.1 km\n"
    "albedo = 0.06\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.511813017807640E-02,  9.237201973592191E+03,  3.656807555240766E+01,  4.609492138445353E+01,  3.146279378366679E+02,  2.458819425955092E+06,  1.305431181552540E-02,  8.351469999813422E+01,  8.523944523314083E+01,  9.378994838500144E+03,  9.520787703408098E+03,  2.757709522242716E+04,\n"
    "parent = mars\n"
    "type = Moo\n"
    "mass = 1.08e+20 kg\n"
    "\n"
    "[deimos]\n"
    "horizons_id = 402\n"
    "radius = 7.8 km\n"
    "albedo = 0.06\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  2.553756468312146E-04,  2.345354252701558E+04,  3.691306039579587E+01,  4.370511610741160E+01,  2.004511099887663E+01,  2.458819597749869E+06,  3.299965245255151E-03,  3.321298509178770E+02,  3.321161670844119E+02,  2.345953352056276E+04,  2.346552451410994E+04,  1.090920580201944E+05,\n"
    "parent = mars\n"
    "type = Moo\n"
    "mass = 1.8e+20 kg\n"
    "\n"
    "[amalthea]\n"
    "horizons_id = 505\n"
    "radius = 131 km\n"
    "albedo = 0.06\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  4.794632921775187E-03,  1.811206655321623E+05,  2.567228034694330E+01,  3.588364423082913E+02,  7.268041099052995E+01,  2.458819437750801E+06,  8.306248458438246E-03,  4.467375220878331E+01,  4.506168563778373E+01,  1.819932563907947E+05,  1.828658472494271E+05,  4.334086583146680E+04,\n"
    "parent = jupiter\n"
    "type = Moo\n"
    "\n"
    "[elara]\n"
    "horizons_id = 507\n"
    "radius = 40 km\n"
    "albedo = 0.03\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  2.092146121668633E-01,  9.226664204234492E+06,  3.758535277034184E+01,  5.660423322868364E+01,  2.394313115021421E+02,  2.458857490493238E+06,  1.618112609328083E-05,  3.068874177318380E+02,  2.847479479963565E+02,  1.166772217366946E+07,  1.410878014310443E+07,  2.224814255353273E+07,\n"
    "parent = jupiter\n"
    "type = Moo\n"
    "\n"
    "[thebe]\n"
    "horizons_id = 514\n"
    "radius = 50 km\n"
    "albedo = 0.05\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.969881799461251E-02,  2.180411271712197E+05,  2.655056749830456E+01,  3.575129116602113E+02,  1.362749287108362E+01,  2.458819540311916E+06,  6.147797060716223E-03,  3.385875332233279E+02,  3.377441473455411E+02,  2.224225892752431E+05,  2.268040513792665E+05,  5.855756077251837E+04,\n"
    "parent = jupiter\n"
    "type = Moo\n"
    "\n"
    "[adrastea]\n"
    "horizons_id = 515\n"
    "radius = 10 km\n"
    "albedo = 0.05\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  6.707050586543179E-03,  1.290018400675079E+05,  2.550036728381007E+01,  3.580280841880729E+02,  1.274855148400927E+02,  2.458819499703035E+06,  1.377875673748157E-02,  3.535319445417623E-01,  3.583143077902437E-01,  1.298729042058378E+05,  1.307439683441676E+05,  2.612717583007415E+04,\n"
    "parent = jupiter\n"
    "type = Moo\n"
    "\n"
    "[metis]\n"
    "horizons_id = 516\n"
    "radius = 20 km\n"
    "albedo = 0.05\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  6.517476904888711E-03,  1.280384566644496E+05,  2.549103228522158E+01,  3.580650175463430E+02,  1.048749620579683E+02,  2.458819501339147E+06,  1.393854888721918E-02,  3.583872778890905E+02,  3.583660863884056E+02,  1.288784187824025E+05,  1.297183809003554E+05,  2.582765271427203E+04,\n"
    "parent = jupiter\n"
    "type = Moo\n"
    "\n"
    "[mimas]\n"
    "horizons_id = 601\n"
    "radius = 198.8 km\n"
    "albedo = 0.6\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.835618046515262E-02,  1.825942428912609E+05,  6.316149763048216E+00,  1.446864601237699E+02,  2.063316847557159E+02,  2.458819814030363E+06,  4.398668754192783E-03,  2.406543369841876E+02,  2.388414920548397E+02,  1.860086512619040E+05,  1.894230596325472E+05,  8.184294388088449E+04,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "mass = 3.75e+19 kg\n"
    "\n"
    "[enceladus]\n"
    "horizons_id = 602\n"
    "radius = 252.3 km\n"
    "albedo = 1.04\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  5.452220675517615E-03,  2.371110657865985E+05,  6.454311039503140E+00,  1.305209149301499E+02,  1.264365916097429E+02,  2.458819274320861E+06,  3.031319720567726E-03,  5.910672584515428E+01,  5.964473846961553E+01,  2.384109348146645E+05,  2.397108038427305E+05,  1.187601550431562E+05,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "mass = 1.0805e+20 kg\n"
    "\n"
    "[tethys]\n"
    "horizons_id = 603\n"
    "radius = 536.3 km\n"
    "albedo = 0.8\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.167942330177171E-03,  2.946293910131489E+05,  6.375975582570282E+00,  1.208255952817582E+02,  3.448784099190250E+02,  2.458819463096958E+06,  2.202651566337655E-03,  7.022984632050730E+00,  7.039372209531971E+00,  2.949739035213690E+05,  2.953184160295891E+05,  1.634393771133632E+05,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "mass = 6.176e+20 kg\n"
    "\n"
    "[dione]\n"
    "horizons_id = 604\n"
    "radius = 562.5 km\n"
    "albedo = 0.6\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  2.734647981610809E-03,  3.766206614332204E+05,  6.481294785403623E+00,  1.307223413933103E+02,  1.182793559124145E+02,  2.458819276534383E+06,  1.520481923641598E-03,  2.935659720113345E+01,  2.951068249872522E+01,  3.776534105701846E+05,  3.786861597071488E+05,  2.367670370837357E+05,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "mass = 1.09572e+21 kg\n"
    "\n"
    "[rhea]\n"
    "horizons_id = 605\n"
    "radius = 764.5 km\n"
    "albedo = 0.6\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.011545573111864E-03,  5.266774525881086E+05,  6.159790679617806E+00,  1.318010548204380E+02,  1.990200152830483E+02,  2.458820037509878E+06,  9.218185744318266E-04,  3.171899586663970E+02,  3.171111134966515E+02,  5.272107502886599E+05,  5.277440479892113E+05,  3.905323780461791E+05,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "mass = 2.309e+21 kg\n"
    "\n"
    "[titan]\n"
    "horizons_id = 606\n"
    "radius = 2575.5 km\n"
    "albedo = 0.2\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  2.862946792903489E-02,  1.186965490839200E+06,  6.338784351197050E+00,  1.270512040930268E+02,  2.175636100724734E+02,  2.458825270307215E+06,  2.612718369233079E-04,  2.297417386425182E+02,  2.272954935598391E+02,  1.221949247635283E+06,  1.256933004431366E+06,  1.377875259114407E+06,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "mass = 1.34553e+23 kg\n"
    "\n"
    "[hyperion]\n"
    "horizons_id = 607\n"
    "radius = 133 km\n"
    "albedo = 0.25\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.301547776549211E-01,  1.290489722241824E+06,  5.848771035079648E+00,  1.228377494077538E+02,  2.285113823118897E+02,  2.458817990232124E+06,  1.952774566988924E-04,  2.547276171820853E+01,  3.296077141938540E+01,  1.483585457608999E+06,  1.676681192976173E+06,  1.843530769427733E+06,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "mass = 1.08e+19 kg\n"
    "\n"
    "[iapetus]\n"
    "horizons_id = 608\n"
    "radius = 734.5 km\n"
    "albedo = 0.6\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  2.774343745088945E-02,  3.462543647964179E+06,  1.521279761820535E+01,  4.749254781941995E+01,  3.252920594089143E+02,  2.458797840975324E+06,  5.250493066160424E-05,  9.825456287477958E+01,  1.013836447999462E+02,  3.561347674410044E+06,  3.660151700855909E+06,  6.856498912839446E+06,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "mass = 1.8059e+21 kg\n"
    "\n"
    "[phoebe]\n"
    "horizons_id = 609\n"
    "radius = 106.6 km\n"
    "albedo = 0.081\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.528492497160765E-01,  1.099800798097620E+07,  1.554237075453701E+02,  1.968680727328412E+02,  3.019035424899350E+02,  2.458632899924659E+06,  7.543813908059313E-06,  1.216232274475678E+02,  1.350532593442134E+02,  1.298235051705992E+07,  1.496669305314365E+07,  4.772121958302812E+07,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "mass = 8.289e+18 kg\n"
    "\n"
    "[janus]\n"
    "horizons_id = 610\n"
    "radius = 97 km\n"
    "albedo = 0.5\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.049819539684966E-02,  1.504690627850379E+05,  6.599123705207988E+00,  1.313984078999776E+02,  3.373231466858949E+02,  2.458819531226046E+06,  5.950795449421885E-03,  3.439451681272043E+02,  3.436082235244609E+02,  1.520654758637707E+05,  1.536618889425035E+05,  6.049611401698805E+04,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "mass = 1.98e+18 kg\n"
    "\n"
    "[epimetheus]\n"
    "horizons_id = 611\n"
    "radius = 69 km\n"
    "albedo = 0.5\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.314332709593876E-02,  1.500231232314442E+05,  6.577973702821606E+00,  1.335193851034444E+02,  1.045969294970623E+02,  2.458819457396768E+06,  5.953396105832801E-03,  2.191397057476341E+01,  2.248475880981203E+01,  1.520211874232611E+05,  1.540192516150780E+05,  6.046968715004404E+04,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "mass = 5.5e+17 kg\n"
    "\n"
    "[helene]\n"
    "horizons_id = 612\n"
    "radius = 16 km\n"
    "albedo = 0.6\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  7.247007053498255E-03,  3.747426430301638E+05,  6.465030163473569E+00,  1.286744025978309E+02,  1.136830034552298E+02,  2.458818686291513E+06,  1.521539018129986E-03,  1.069709079113073E+02,  1.077630702994960E+02,  3.774782304286220E+05,  3.802138178270801E+05,  2.366025423669056E+05,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "\n"
    "[telesto]\n"
    "horizons_id = 613\n"
    "radius = 15 km\n"
    "albedo = 1\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.118398586322094E-03,  2.946338437811428E+05,  5.775365164984819E+00,  1.215303293186315E+02,  4.599420262602134E+01,  2.458819469401365E+06,  2.202764318334707E-03,  5.823496586240680E+00,  5.836518260191882E+00,  2.949637308006866E+05,  2.952936178202304E+05,  1.634310112087528E+05,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "\n"
    "[calypso]\n"
    "horizons_id = 614\n"
    "radius = 15 km\n"
    "albedo = 0.7\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.352109431501499E-03,  2.946107593198694E+05,  7.601853177637527E+00,  1.225554229655063E+02,  3.064144421979695E+02,  2.458819586582179E+06,  2.202250097326984E-03,  3.435256270651951E+02,  3.434816167655747E+02,  2.950096446427748E+05,  2.954085299656802E+05,  1.634691720240838E+05,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "\n"
    "[atlas]\n"
    "horizons_id = 615\n"
    "radius = 18.5 km\n"
    "albedo = 0.5\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  4.233165749952221E-03,  1.377365028573421E+05,  6.462286779237711E+00,  1.305501201293694E+02,  5.151930341317753E+01,  2.458819522856731E+06,  6.859364277393887E-03,  3.464539792759045E+02,  3.463397722787413E+02,  1.383220429922001E+05,  1.389075831270581E+05,  5.248299775920001E+04,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "\n"
    "[prometheus]\n"
    "horizons_id = 616\n"
    "radius = 74 km\n"
    "albedo = 0.5\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  5.572081595665974E-03,  1.392477498550550E+05,  6.456634846083831E+00,  1.306149668776635E+02,  2.041757497586378E+02,  2.458819539343555E+06,  6.734395890510948E-03,  3.371078817843327E+02,  3.368578989814603E+02,  1.400279972815857E+05,  1.408082447081165E+05,  5.345691073898038E+04,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "mass = 1.4e+17 kg\n"
    "\n"
    "[pandora]\n"
    "horizons_id = 617\n"
    "radius = 55 km\n"
    "albedo = 0.5\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  8.110077569449953E-03,  1.412008433908263E+05,  6.501547895451425E+00,  1.308621238350825E+02,  6.751000138078474E+01,  2.458819463976146E+06,  6.569922403204627E-03,  2.044862714221557E+01,  2.077642115841814E+01,  1.423553563734416E+05,  1.435098693560569E+05,  5.479516772137247E+04,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "mass = 1.3e+17 kg\n"
    "\n"
    "[pan]\n"
    "horizons_id = 618\n"
    "radius = 10 km\n"
    "albedo = 0.5\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  5.063373363055096E-03,  1.335837779687298E+05,  6.463086855014238E+00,  1.305778530527385E+02,  2.144025853995925E+02,  2.458819499915358E+06,  7.172713844614596E-03,  5.245468873795184E-02,  5.298926660476381E-02,  1.342636047285400E+05,  1.349434314883502E+05,  5.019020802987903E+04,\n"
    "parent = saturn\n"
    "type = Moo\n"
    "\n"
    "[ariel]\n"
    "horizons_id = 701\n"
    "radius = 581.1 km\n"
    "albedo = 0.34\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  6.151611972643227E-04,  1.908241656272896E+05,  7.479977418682309E+01,  1.673124344507994E+02,  1.904984096480188E+02,  2.458818726122323E+06,  1.652959673349232E-03,  1.105218943284868E+02,  1.105878952424333E+02,  1.909416255062436E+05,  1.910590853851976E+05,  2.177911571614853E+05,\n"
    "parent = uranus\n"
    "type = Moo\n"
    "mass = 1.353e+21 kg\n"
    "\n"
    "[umbriel]\n"
    "horizons_id = 702\n"
    "radius = 584.7 km\n"
    "albedo = 0.18\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  3.456661988039283E-03,  2.650833690166083E+05,  7.480063180491165E+01,  1.673552047461912E+02,  4.195415444123024E+01,  2.458819765730345E+06,  1.005272507801937E-03,  3.369198461495606E+02,  3.367639465409144E+02,  2.660028509602326E+05,  2.669223329038569E+05,  3.581118524639177E+05,\n"
    "parent = uranus\n"
    "type = Moo\n"
    "mass = 1.172e+21 kg\n"
    "\n"
    "[titania]\n"
    "horizons_id = 703\n"
    "radius = 788.9 km\n"
    "albedo = 0.27\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.186218146212593E-03,  4.357753425201614E+05,  7.488360523622866E+01,  1.672950406227273E+02,  2.716634671640432E+02,  2.458822867405731E+06,  4.785764994521202E-04,  2.207611082441932E+02,  2.206724578690607E+02,  4.362928810527296E+05,  4.368104195852977E+05,  7.522308354299305E+05,\n"
    "parent = uranus\n"
    "type = Moo\n"
    "mass = 3.527e+21 kg\n"
    "\n"
    "[oberon]\n"
    "horizons_id = 704\n"
    "radius = 761.4 km\n"
    "albedo = 0.24\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  2.334872685125609E-03,  5.821692268122704E+05,  7.500548754890892E+01,  1.674049967832449E+02,  2.009992423627299E+02,  2.458822943841365E+06,  3.094005411058702E-04,  2.679385206044721E+02,  2.676711661041973E+02,  5.835316990372573E+05,  5.848941712622443E+05,  1.163540305111541E+06,\n"
    "parent = uranus\n"
    "type = Moo\n"
    "mass = 3.014e+21 kg\n"
    "\n"
    "[miranda]\n"
    "horizons_id = 705\n"
    "radius = 240 km\n"
    "albedo = 0.27\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.310416100010900E-03,  1.297038833380822E+05,  7.722800609806824E+01,  1.711201510728253E+02,  2.844000619089552E+02,  2.458820007713669E+06,  2.946636249554277E-03,  2.307414960151142E+02,  2.306253457692237E+02,  1.298740724135469E+05,  1.300442614890116E+05,  1.221732068403270E+05,\n"
    "parent = uranus\n"
    "type = Moo\n"
    "mass = 6.59e+19 kg\n"
    "\n"
    "[cordelia]\n"
    "horizons_id = 706\n"
    "radius = 13 km\n"
    "albedo = 0.07\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  4.927693182629693E-03,  4.978237569192627E+04,  7.466947972818501E+01,  1.673815392983380E+02,  3.283312444278594E+01,  2.458819498918130E+06,  1.232476327203785E-02,  1.152040099207867E+00,  1.163463466259401E+00,  5.002890277506540E+04,  5.027542985820453E+04,  2.920948597988571E+04,\n"
    "parent = uranus\n"
    "type = Moo\n"
    "\n"
    "[ophelia]\n"
    "horizons_id = 707\n"
    "radius = 16 km\n"
    "albedo = 0.07\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.158959629645492E-02,  5.352243851452583E+04,  7.478547389432953E+01,  1.672644305803320E+02,  3.264031768887420E+02,  2.458819427733432E+06,  1.094490956886339E-02,  6.833817094082056E+01,  6.957898817849498E+01,  5.415001533166669E+04,  5.477759214880755E+04,  3.289200314858200E+04,\n"
    "parent = uranus\n"
    "type = Moo\n"
    "\n"
    "[bianca]\n"
    "horizons_id = 708\n"
    "radius = 22 km\n"
    "albedo = 0.07\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.619664203930312E-03,  5.924833228616579E+04,  7.484465284778791E+01,  1.674132538669879E+02,  3.117815210863786E+02,  2.458819507996767E+06,  9.539813526156857E-03,  3.534087457989922E+02,  3.533873986952416E+02,  5.934445036812896E+04,  5.944056845009214E+04,  3.773658667571746E+04,\n"
    "parent = uranus\n"
    "type = Moo\n"
    "\n"
    "[cressida]\n"
    "horizons_id = 709\n"
    "radius = 33 km\n"
    "albedo = 0.07\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.605439575648428E-03,  6.174472706867354E+04,  7.483362005140667E+01,  1.672557077401938E+02,  8.106528235501730E+01,  2.458819508768508E+06,  8.967338211243325E-03,  3.532063530110721E+02,  3.531845470603486E+02,  6.184401389609930E+04,  6.194330072352506E+04,  4.014569223547618E+04,\n"
    "parent = uranus\n"
    "type = Moo\n"
    "\n"
    "[desdemona]\n"
    "horizons_id = 710\n"
    "radius = 29 km\n"
    "albedo = 0.07\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  8.703770310639157E-04,  6.263191811384931E+04,  7.475641809297301E+01,  1.672809313829226E+02,  3.845488568056164E+01,  2.458819471183210E+06,  8.787174244846808E-03,  2.187804869796263E+01,  2.191525180117555E+01,  6.268647898531640E+04,  6.274103985678350E+04,  4.096880179781573E+04,\n"
    "parent = uranus\n"
    "type = Moo\n"
    "\n"
    "[juliet]\n"
    "horizons_id = 711\n"
    "radius = 42 km\n"
    "albedo = 0.07\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  8.190076035075316E-04,  6.438985256817123E+04,  7.479897189388346E+01,  1.673010405078851E+02,  3.193665503726690E+02,  2.458819548627645E+06,  8.430438277788509E-03,  3.245801157700184E+02,  3.245256774917114E+02,  6.444263157341989E+04,  6.449541057866856E+04,  4.270240622584049E+04,\n"
    "parent = uranus\n"
    "type = Moo\n"
    "\n"
    "[portia]\n"
    "horizons_id = 712\n"
    "radius = 55 km\n"
    "albedo = 0.07\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  6.381469912408671E-04,  6.609967403385893E+04,  7.483788798525697E+01,  1.672730071403574E+02,  2.772588511170728E+02,  2.458819470595187E+06,  8.107654409558110E-03,  2.059811093492476E+01,  2.062385676412121E+01,  6.614188227702901E+04,  6.618409052019910E+04,  4.440248459228802E+04,\n"
    "parent = uranus\n"
    "type = Moo\n"
    "\n"
    "[rosalind]\n"
    "horizons_id = 713\n"
    "radius = 29 km\n"
    "albedo = 0.07\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.028480489921392E-03,  6.994616809265653E+04,  7.478018087508886E+01,  1.672991939676257E+02,  4.874088060677504E+01,  2.458819504398126E+06,  7.443785032859952E-03,  3.571713760371118E+02,  3.571655525540463E+02,  7.001818042516361E+04,  7.009019275767068E+04,  4.836249279241821E+04,\n"
    "parent = uranus\n"
    "type = Moo\n"
    "\n"
    "[belinda]\n"
    "horizons_id = 714\n"
    "radius = 34 km\n"
    "albedo = 0.07\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  9.529275045555424E-04,  7.525279169203488E+04,  7.481428079069224E+01,  1.673227831098457E+02,  3.076856947725461E+02,  2.458819516821668E+06,  6.671216839437399E-03,  3.503041056564243E+02,  3.502856931414453E+02,  7.532457054707802E+04,  7.539634940212117E+04,  5.396316873884731E+04,\n"
    "parent = uranus\n"
    "type = Moo\n"
    "\n"
    "[puck]\n"
    "horizons_id = 715\n"
    "radius = 77 km\n"
    "albedo = 0.07\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  9.222146235867687E-04,  8.599794433587370E+04,  7.505138794236844E+01,  1.675625440418152E+02,  1.528991027096699E+02,  2.458819502565501E+06,  5.461059420287037E-03,  3.587895052807125E+02,  3.587872701991024E+02,  8.607732610476675E+04,  8.615670787365979E+04,  6.592127503001572E+04,\n"
    "parent = uranus\n"
    "type = Moo\n"
    "\n"
    "[triton]\n"
    "horizons_id = 801\n"
    "radius = 1352.6 km\n"
    "albedo = 0.7\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  2.501689970480060E-05,  3.547585763438361E+05,  1.104486454497699E+02,  2.128194722424926E+02,  1.250781707227524E+01,  2.458819727521693E+06,  7.089649254756315E-04,  3.460632566482993E+02,  3.460625661750459E+02,  3.547674515255895E+05,  3.547763267073428E+05,  5.077825250078241E+05,\n"
    "parent = neptune\n"
    "type = Moo\n"
    "mass = 2.147e+22 kg\n"
    "\n"
    "[nereid]\n"
    "horizons_id = 802\n"
    "radius = 170 km\n"
    "albedo = 0.2\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  7.448339140463651E-01,  1.406251029208839E+06,  2.743957624651974E+01,  3.530024190880220E+02,  2.625738026624624E+02,  2.458891095073930E+06,  1.157804414087614E-05,  2.883803679733778E+02,  2.092606204434716E+02,  5.511120429477302E+06,  9.615989829745766E+06,  3.109333455803857E+07,\n"
    "parent = neptune\n"
    "type = Moo\n"
    "\n"
    "[naiad]\n"
    "horizons_id = 803\n"
    "radius = 29 km\n"
    "albedo = 0.06\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.530163580942538E-03,  4.822013797740581E+04,  4.238077307818315E+01,  3.048617816984467E+01,  2.128011827884681E+02,  2.458819500926885E+06,  1.411415245204852E-02,  3.588696985082939E+02,  3.588662330136309E+02,  4.829403575209039E+04,  4.836793352677498E+04,  2.550631369634596E+04,\n"
    "parent = neptune\n"
    "type = Moo\n"
    "\n"
    "[thalassa]\n"
    "horizons_id = 804\n"
    "radius = 40 km\n"
    "albedo = 0.06\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.359633828514774E-03,  5.007143747093548E+04,  4.719241740431508E+01,  2.934672136543370E+01,  6.663115368387348E+01,  2.458819506555571E+06,  1.334208302730170E-02,  3.524430267010251E+02,  3.524225022194724E+02,  5.013960897945244E+04,  5.020778048796939E+04,  2.698229348920537E+04,\n"
    "parent = neptune\n"
    "type = Moo\n"
    "\n"
    "[despina]\n"
    "horizons_id = 805\n"
    "radius = 74 km\n"
    "albedo = 0.06\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.404924475558423E-03,  5.251423753832035E+04,  4.702545366783426E+01,  2.950239088168049E+01,  2.294690511757149E+02,  2.458819506408707E+06,  1.242120091599349E-02,  3.531222282937998E+02,  3.531029155032180E+02,  5.258811987505641E+04,  5.266200221179248E+04,  2.898270484752126E+04,\n"
    "parent = neptune\n"
    "type = Moo\n"
    "\n"
    "[galatea]\n"
    "horizons_id = 806\n"
    "radius = 79 km\n"
    "albedo = 0.06\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  7.424356849806297E-04,  6.195902646056462E+04,  4.706465555497289E+01,  2.949583853003638E+01,  9.238870160507044E+01,  2.458819514649051E+06,  9.701845224604483E-03,  3.477205877107563E+02,  3.477024772014573E+02,  6.200506123067167E+04,  6.205109600077872E+04,  3.710634334662623E+04,\n"
    "parent = neptune\n"
    "type = Moo\n"
    "\n"
    "[larissa]\n"
    "horizons_id = 807\n"
    "radius = 104 km\n"
    "albedo = 0.06\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.791086007806864E-03,  7.346086247088744E+04,  4.729827781484587E+01,  2.938140348667610E+01,  1.640216429340891E+02,  2.458819527700107E+06,  7.503142321998863E-03,  3.420428104382489E+02,  3.419793975931926E+02,  7.359267327827326E+04,  7.372448408565907E+04,  4.797989756165184E+04,\n"
    "parent = neptune\n"
    "type = Moo\n"
    "\n"
    "[proteus]\n"
    "horizons_id = 808\n"
    "radius = 218 km\n"
    "albedo = 0.06\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  6.420274957697180E-04,  1.176000816559778E+05,  4.758145788124170E+01,  2.964729528271211E+01,  2.022898082196735E+02,  2.458819620922638E+06,  3.710780282377332E-03,  3.212308217880102E+02,  3.211847239737984E+02,  1.176756326477197E+05,  1.177511836394616E+05,  9.701463643904134E+04,\n"
    "parent = neptune\n"
    "type = Moo\n"
    "\n"
    "[charon]\n"
    "horizons_id = 901\n"
    "radius = 664 km\n"
    "albedo = 0.372\n"
    "orbit = horizons:2458819.500000000, A.D. 2019-Dec-02 00:00:00.0000,  1.477950961194875E-04,  1.959394937140057E+04,  9.625364114377922E+01,  2.230146242471567E+02,  1.721950077414197E+02,  2.458817705436320E+06,  6.523445586984761E-04,  1.011462208239336E+02,  1.011628368362798E+02,  1.959684568909282E+04,  1.959974200678508E+04,  5.518556032999697E+05,\n"
    "parent = pluto\n"
    "type = Pla\n"
    "mass = 1.53e+21 kg\n"
    "\n"
    "";

ASSET_REGISTER(planets_ini, "planets.ini", DATA_planets_ini, false)

