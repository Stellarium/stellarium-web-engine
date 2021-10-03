import fetch from "node-fetch";
import fs from "fs";
import zlib from "zlib";

// TODO: Move to secrets at least.
const COOKIE = "";
const LOGIN = "";
const PASSWORD = "";

const downloadOptions = { headers: { Cookie: COOKIE } };
const loginOptions = {
  method: "POST",
  headers: { "Content-Type": "application/json", Cookie: COOKIE },
  body: JSON.stringify({ identity: LOGIN, password: PASSWORD }),
};

const INPUT_FILE_NAME = "track.json";
const OUTPUT_FILE_NAME = "tle_satellite.jsonl.gz";
const LOGIN_URL = "https://www.space-track.org/ajaxauth/login";
const ALL_DEBRIS_URL =
  "https://www.space-track.org/basicspacedata/query/class/gp/object_type/debris/decay_date/null-val/epoch/%3Enow-30/orderby/norad_cat_id/format/json";

const transform = (item) => ({
  types: ["Asa"],
  model: "tle_satellite",
  model_data: {
    norad_number: +item.NORAD_CAT_ID,
    designation: `${item.NORAD_CAT_ID}${item.CLASSIFICATION_TYPE}`,
    tle: [item.TLE_LINE1, item.TLE_LINE2],
    group: [item.OBJECT_TYPE],
    status: "Operational",
    owner: item.COUNTRY_CODE,
    launch_site: item.SITE,
    launch_date: item.LAUNCH_DATE
  },
  names: [`NAME ${item.OBJECT_NAME}`],
    short_name: item.OBJECT_NAME,
    interest: 8.8,
});

const saveDebrisToJsonl = (items) => {
  const jsonl = items.map((i) => JSON.stringify(i)).join("\n");
  zlib.gzip(jsonl, (err, data) => fs.writeFileSync(OUTPUT_FILE_NAME, data));
};

const readDebrisFromJson = () => Promise.resolve(JSON.parse(fs.readFileSync(INPUT_FILE_NAME)));
const fetchDebrisFromApi = async () => {
  try {
    const login = await fetch(LOGIN_URL, loginOptions);
    const response = await fetch(ALL_DEBRIS_URL, downloadOptions);
    const items = await response.json();
    return items;
  } catch (error) {
    console.log(`Error while fetching data `, e);
    return [];
  }
};

const downloadDebrisToJsonl = async (isFromFile = true) => {
  const query = isFromFile ? readDebrisFromJson : fetchDebrisFromApi;
  const items = await query();
  saveDebrisToJsonl(items.map(transform));
};

downloadDebrisToJsonl(true);
