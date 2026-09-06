#include <Arduino.h>
#include <DNSServer.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include <U8g2lib.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>

#include "Keyboard.h"

namespace {
constexpr uint8_t OLED_CS = 5;
constexpr uint8_t OLED_DC = 16;
constexpr uint8_t OLED_RST = 17;
constexpr uint8_t SD_CS = 4;
constexpr uint32_t EXIT_HOLD_MS = 1000;
constexpr uint32_t DISPLAY_INTERVAL_MS = 500;
constexpr uint8_t MAX_LOGIN_ATTEMPTS = 5;
constexpr uint32_t LOGIN_LOCKOUT_MS = 30000;
constexpr char MAIN_FIRMWARE[] = "/main.bin";
constexpr char TEMP_SUFFIX[] = ".sdm.part";
constexpr char BACKUP_SUFFIX[] = ".sdm.bak";
constexpr char SESSION_COOKIE[] = "SCISESSION";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_MASK(255, 255, 255, 0);

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI display(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
Keyboard keyboard;
WebServer server(80);
DNSServer dnsServer;

String apSsid;
String sessionToken;
char apPassword[9] = "00000000";
char webPin[7] = "000000";
String activity = "Ready";
File uploadFile;
String uploadFinalPath;
String uploadTempPath;
String uploadError;
size_t uploadBytes = 0;
uint32_t lastDisplay = 0;
uint32_t returnAt = 0;
uint32_t loginBlockedUntil = 0;
uint8_t failedLoginAttempts = 0;
bool sdReady = false;
bool uploadActive = false;
bool uploadAuthorized = false;
bool uploadSucceeded = false;
bool exitTriggered = false;

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>SCI-CALC SD Manager</title>
<style>
:root{color-scheme:light;--bg:#f3f5f7;--surface:#fff;--ink:#182026;--muted:#66717a;--line:#d6dce1;--accent:#087e8b;--accent2:#d1495b;--ok:#2d7d46;--hover:#edf6f7}
*{box-sizing:border-box}
[hidden]{display:none!important}
body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.4 system-ui,-apple-system,"Segoe UI",sans-serif;letter-spacing:0}
header{height:52px;background:#17252d;color:#fff;display:flex;align-items:center;justify-content:space-between;padding:0 18px}
h1{font-size:18px;margin:0;font-weight:650}
h2{font-size:19px;margin:0 0 18px;font-weight:650}
main{max-width:980px;margin:0 auto;padding:16px}
.status{display:flex;align-items:center;gap:7px;font-size:13px}
.dot{width:8px;height:8px;border-radius:50%;background:#aeb7bd}.dot.online{background:#69d28b}
.loginView{min-height:calc(100vh - 84px);display:grid;place-items:center}
.loginPanel{width:min(100%,340px);background:var(--surface);border:1px solid var(--line);border-radius:7px;padding:22px}
.loginPanel label{display:block;color:var(--muted);font-size:13px;margin-bottom:6px}
.pin{width:100%;height:48px;border:1px solid var(--line);border-radius:6px;background:var(--surface);color:var(--ink);font:24px/1 ui-monospace,"Cascadia Mono",monospace;letter-spacing:0;text-align:center}
.pin:focus{outline:2px solid var(--accent);outline-offset:1px}
.loginPanel button{width:100%;margin-top:12px}
.pathbar,.toolbar,.storage{display:flex;align-items:center;gap:8px}
.pathbar{min-height:40px;overflow-x:auto;white-space:nowrap;border-bottom:1px solid var(--line)}
.crumb{border:0;background:transparent;color:var(--accent);padding:7px 4px;cursor:pointer;font:inherit}
.sep{color:var(--muted)}
.toolbar{padding:12px 0;flex-wrap:wrap}
button,.uploadLabel{min-height:36px;border:1px solid var(--line);border-radius:6px;background:var(--surface);color:var(--ink);padding:7px 12px;font:inherit;cursor:pointer}
button:hover,.uploadLabel:hover{background:var(--hover)}
.primary{background:var(--accent);border-color:var(--accent);color:#fff}
.primary:hover{background:#066c77}
.danger{color:var(--accent2)}
.spacer{flex:1}
.storage{font-size:13px;color:var(--muted);min-width:220px}
.meter{width:110px;height:7px;background:#d9dfe3;border-radius:4px;overflow:hidden}
.meter>i{display:block;height:100%;width:0;background:var(--ok)}
.files{background:var(--surface);border:1px solid var(--line);border-radius:7px;overflow:hidden}
.head,.row{display:grid;grid-template-columns:minmax(180px,1fr) 120px 104px 116px;align-items:center;min-height:44px;padding:0 12px;border-bottom:1px solid var(--line)}
.head{min-height:34px;color:var(--muted);font-size:12px;background:#eef1f3}
.row:last-child{border-bottom:0}
.row:hover{background:var(--hover)}
.name{display:flex;align-items:center;gap:9px;min-width:0}
.nameText{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.kind{display:inline-grid;place-items:center;width:34px;height:22px;border-radius:4px;background:#e4ecef;color:#34505c;font-size:10px;font-weight:700;flex:0 0 auto}
.folder{cursor:pointer;color:var(--accent);font-weight:600}
.actions{display:flex;justify-content:flex-end;gap:5px}
.icon{width:32px;min-height:32px;padding:0;font-size:17px}
.empty{padding:42px;text-align:center;color:var(--muted)}
.notice{min-height:22px;padding:2px 0;color:var(--muted);font-size:13px}
.notice.error{color:var(--accent2)}
.drop{outline:2px solid var(--accent);outline-offset:-4px}
input[type=file]{display:none}
@media(max-width:680px){
 main{padding:10px}
 .storage{order:3;width:100%}
 .head{display:none}
 .row{grid-template-columns:minmax(120px,1fr) auto;grid-template-areas:"name actions" "size actions";padding:8px 9px}
 .name{grid-area:name}.size{grid-area:size;color:var(--muted);font-size:12px;margin-left:43px}.modified{display:none}.actions{grid-area:actions}
 .uploadLabel,button{padding:7px 10px}
}
</style>
</head>
<body>
<header><h1>SCI-CALC SD Manager</h1><div class="status"><span class="dot" id="statusDot"></span><span id="connection">Locked</span></div></header>
<main>
  <section class="loginView" id="loginView">
    <form class="loginPanel" id="loginForm">
      <h2>Unlock SD card</h2>
      <label for="pin">Device code</label>
      <input class="pin" id="pin" type="password" inputmode="numeric" pattern="[0-9]{6}" maxlength="6" autocomplete="one-time-code" required autofocus>
      <button class="primary" type="submit">Unlock</button>
      <div class="notice" id="loginNotice" role="status"></div>
    </form>
  </section>
  <section id="managerView" hidden>
    <div id="dropZone">
      <nav class="pathbar" id="breadcrumbs"></nav>
      <div class="toolbar">
        <label class="uploadLabel primary" for="files">Upload</label>
        <input id="files" type="file" multiple>
        <button id="mkdir">New folder</button>
        <button id="refresh">Refresh</button>
        <div class="spacer"></div>
        <div class="storage"><div class="meter"><i id="meter"></i></div><span id="storageText">Reading SD...</span></div>
        <button id="lock">Lock</button>
        <button id="return">Return to calculator</button>
      </div>
      <div class="notice" id="notice"></div>
      <section class="files">
        <div class="head"><span>Name</span><span>Size</span><span>Type</span><span></span></div>
        <div id="rows"><div class="empty">Loading...</div></div>
      </section>
    </div>
  </section>
</main>
<script>
(function(){
  var current="/", token="", busy=false;
  var rows=document.getElementById("rows"), notice=document.getElementById("notice"), loginNotice=document.getElementById("loginNotice");
  var loginView=document.getElementById("loginView"), managerView=document.getElementById("managerView"), pin=document.getElementById("pin");
  function show(message,error){notice.textContent=message||"";notice.className="notice"+(error?" error":"");}
  function showLogin(message,error){token="";managerView.hidden=true;loginView.hidden=false;document.getElementById("connection").textContent="Locked";document.getElementById("statusDot").className="dot";loginNotice.textContent=message||"";loginNotice.className="notice"+(error?" error":"");pin.value="";setTimeout(function(){pin.focus()},0);}
  function showManager(status){loginView.hidden=true;managerView.hidden=false;document.getElementById("statusDot").className="dot online";document.getElementById("connection").textContent=status.clients+" client"+(status.clients===1?"":"s");}
  function handleError(error){if(error.status===401)showLogin("Session expired",true);else show(error.message,true);}
  function join(dir,name){return (dir==="/"?"/":dir+"/")+name;}
  function parent(path){var i=path.lastIndexOf("/");return i<=0?"/":path.slice(0,i);}
  function basename(path){var i=path.lastIndexOf("/");return path.slice(i+1);}
  function human(bytes){if(bytes<1024)return bytes+" B";if(bytes<1048576)return(bytes/1024).toFixed(1)+" KB";if(bytes<1073741824)return(bytes/1048576).toFixed(1)+" MB";return(bytes/1073741824).toFixed(2)+" GB";}
  function actionButton(title,symbol,fn,danger){var b=document.createElement("button");b.className="icon"+(danger?" danger":"");b.title=title;b.setAttribute("aria-label",title);b.textContent=symbol;b.onclick=function(e){e.stopPropagation();fn();};return b;}
  async function request(url,options){
    options=options||{};options.headers=options.headers||{};
    if(token)options.headers["X-Sci-Token"]=token;
    var response=await fetch(url,options), data;
    try{data=await response.json();}catch(e){data={error:"Invalid response"};}
    if(!response.ok){var error=new Error(data.error||("HTTP "+response.status));error.status=response.status;throw error;}
    return data;
  }
  function renderBreadcrumbs(){
    var nav=document.getElementById("breadcrumbs");nav.textContent="";
    var root=document.createElement("button");root.className="crumb";root.textContent="SD";root.onclick=function(){load("/")};nav.appendChild(root);
    var parts=current.split("/").filter(Boolean), built="";
    parts.forEach(function(part){built+="/"+part;var target=built,s=document.createElement("span");s.className="sep";s.textContent="/";nav.appendChild(s);var b=document.createElement("button");b.className="crumb";b.textContent=part;b.onclick=function(){load(target)};nav.appendChild(b);});
  }
  function renderFiles(items){
    rows.textContent="";
    items.sort(function(a,b){if(a.directory!==b.directory)return a.directory?-1:1;return a.name.localeCompare(b.name);});
    if(!items.length){var empty=document.createElement("div");empty.className="empty";empty.textContent="This folder is empty";rows.appendChild(empty);return;}
    items.forEach(function(item){
      var row=document.createElement("div");row.className="row";
      var name=document.createElement("div");name.className="name"+(item.directory?" folder":"");
      var kind=document.createElement("span");kind.className="kind";kind.textContent=item.directory?"DIR":"FILE";
      var label=document.createElement("span");label.className="nameText";label.textContent=item.name;name.appendChild(kind);name.appendChild(label);
      if(item.directory)name.onclick=function(){load(item.path)};
      var size=document.createElement("div");size.className="size";size.textContent=item.directory?"--":human(item.size);
      var type=document.createElement("div");type.className="modified";type.textContent=item.directory?"Folder":"File";
      var actions=document.createElement("div");actions.className="actions";
      if(!item.directory)actions.appendChild(actionButton("Download","\u2193",function(){location.href="/api/download?path="+encodeURIComponent(item.path)},false));
      actions.appendChild(actionButton("Rename","\u270e",function(){renameItem(item)},false));
      actions.appendChild(actionButton("Delete","\u00d7",function(){deleteItem(item)},true));
      row.appendChild(name);row.appendChild(size);row.appendChild(type);row.appendChild(actions);rows.appendChild(row);
    });
  }
  async function load(path){
    if(busy)return;show("Loading...");
    try{var data=await request("/api/list?path="+encodeURIComponent(path));current=data.path;renderBreadcrumbs();renderFiles(data.items);var percent=data.total?Math.min(100,data.used*100/data.total):0;document.getElementById("meter").style.width=percent+"%";document.getElementById("storageText").textContent=human(data.used)+" / "+human(data.total);show(data.items.length+" item"+(data.items.length===1?"":"s"));}
    catch(e){handleError(e);}
  }
  async function createFolder(){
    var name=prompt("Folder name");if(!name)return;
    try{await request("/api/mkdir",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"path="+encodeURIComponent(join(current,name))});show("Folder created");load(current);}catch(e){handleError(e);}
  }
  async function renameItem(item){
    var name=prompt("New name",item.name);if(!name||name===item.name)return;
    try{await request("/api/rename",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"from="+encodeURIComponent(item.path)+"&to="+encodeURIComponent(join(parent(item.path),name))});show("Renamed");load(current);}catch(e){handleError(e);}
  }
  async function deleteItem(item){
    if(!confirm("Delete "+item.name+(item.directory?" and everything inside it":"")+"?"))return;
    try{await request("/api/delete",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"path="+encodeURIComponent(item.path)});show("Deleted");load(current);}catch(e){handleError(e);}
  }
  function uploadOne(file){
    return new Promise(function(resolve,reject){
      var xhr=new XMLHttpRequest(), form=new FormData();form.append("file",file,file.name);
      xhr.open("POST","/api/upload?dir="+encodeURIComponent(current));xhr.setRequestHeader("X-Sci-Token",token);
      xhr.upload.onprogress=function(e){if(e.lengthComputable)show("Uploading "+file.name+"  "+Math.round(e.loaded*100/e.total)+"%");};
      xhr.onload=function(){var data={};try{data=JSON.parse(xhr.responseText)}catch(e){}if(xhr.status>=200&&xhr.status<300)resolve(data);else{var error=new Error(data.error||"Upload failed");error.status=xhr.status;reject(error);}};
      xhr.onerror=function(){reject(new Error("Connection lost during upload"));};xhr.send(form);
    });
  }
  async function uploadFiles(files){
    if(!files.length||busy)return;busy=true;
    try{for(var i=0;i<files.length;i++)await uploadOne(files[i]);show(files.length+" file"+(files.length===1?"":"s")+" uploaded");}
    catch(e){handleError(e);}finally{busy=false;document.getElementById("files").value="";if(!managerView.hidden)load(current);}
  }
  async function login(event){
    event.preventDefault();var code=pin.value;if(!/^\d{6}$/.test(code))return;
    loginNotice.textContent="Checking...";loginNotice.className="notice";
    try{var response=await fetch("/api/login",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"pin="+encodeURIComponent(code)}),data;try{data=await response.json()}catch(e){data={error:"Invalid response"}}if(!response.ok){var error=new Error(data.error||"Login failed");error.status=response.status;throw error;}token=data.token;var status=await request("/api/status");showManager(status);await load("/");}
    catch(e){showLogin(e.message,true);}
  }
  async function resume(){
    try{var status=await request("/api/status");showManager(status);await load("/");}
    catch(e){showLogin("",false);}
  }
  document.getElementById("loginForm").onsubmit=login;
  pin.oninput=function(){this.value=this.value.replace(/\D/g,"").slice(0,6)};
  document.getElementById("files").onchange=function(){uploadFiles(this.files)};
  document.getElementById("mkdir").onclick=createFolder;
  document.getElementById("refresh").onclick=function(){load(current)};
  document.getElementById("lock").onclick=async function(){try{await request("/api/logout",{method:"POST"});showLogin("Manager locked; new code generated",false);}catch(e){handleError(e);}};
  document.getElementById("return").onclick=async function(){if(!confirm("Return to the calculator firmware?"))return;try{await request("/api/exit",{method:"POST"});show("Returning to calculator...");}catch(e){handleError(e);}};
  var zone=document.getElementById("dropZone");
  ["dragenter","dragover"].forEach(function(type){zone.addEventListener(type,function(e){e.preventDefault();zone.classList.add("drop")})});
  ["dragleave","drop"].forEach(function(type){zone.addEventListener(type,function(e){e.preventDefault();zone.classList.remove("drop")})});
  zone.addEventListener("drop",function(e){uploadFiles(e.dataTransfer.files)});
  setInterval(async function(){if(managerView.hidden)return;try{var data=await request("/api/status");showManager(data);}catch(e){if(e.status===401)showLogin("Session expired",true);}},5000);
  resume();
})();
</script>
</body>
</html>
)HTML";

String jsonEscape(const String &value) {
    String escaped;
    escaped.reserve(value.length() + 8);
    const char hex[] = "0123456789ABCDEF";
    for (size_t index = 0; index < value.length(); ++index) {
        const uint8_t byte = static_cast<uint8_t>(value[index]);
        switch (byte) {
            case '"': escaped += F("\\\""); break;
            case '\\': escaped += F("\\\\"); break;
            case '\b': escaped += F("\\b"); break;
            case '\f': escaped += F("\\f"); break;
            case '\n': escaped += F("\\n"); break;
            case '\r': escaped += F("\\r"); break;
            case '\t': escaped += F("\\t"); break;
            default:
                if (byte < 0x20) {
                    escaped += F("\\u00");
                    escaped += hex[byte >> 4];
                    escaped += hex[byte & 0x0F];
                } else {
                    escaped += static_cast<char>(byte);
                }
        }
    }
    return escaped;
}

String uint64Text(uint64_t value) {
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(value));
    return String(buffer);
}

bool validSegment(const String &segment) {
    if (!segment.length() || segment == "." || segment == "..") return false;
    for (size_t index = 0; index < segment.length(); ++index) {
        const uint8_t value = static_cast<uint8_t>(segment[index]);
        if (value < 0x20 || value == ':' || value == '*' || value == '?' ||
            value == '"' || value == '<' || value == '>' || value == '|') return false;
    }
    return true;
}

bool normalizePath(const String &input, String &output) {
    if (input.length() > 230) return false;
    String candidate = input;
    candidate.replace('\\', '/');
    if (!candidate.startsWith("/")) candidate = "/" + candidate;
    output = "";
    int start = 1;
    while (start <= static_cast<int>(candidate.length())) {
        int end = candidate.indexOf('/', start);
        if (end < 0) end = candidate.length();
        const String segment = candidate.substring(start, end);
        if (segment.length()) {
            if (!validSegment(segment)) return false;
            output += "/";
            output += segment;
        }
        start = end + 1;
    }
    if (!output.length()) output = "/";
    return output.length() <= 230;
}

String baseName(const String &path) {
    const int slash = path.lastIndexOf('/');
    return slash < 0 ? path : path.substring(slash + 1);
}

bool cleanUploadName(const String &input, String &output) {
    String candidate = input;
    candidate.replace('\\', '/');
    output = baseName(candidate);
    return validSegment(output) && output.length() <= 120;
}

String joinPath(const String &directory, const String &name) {
    return directory == "/" ? "/" + name : directory + "/" + name;
}

bool isProtectedPath(const String &path) {
    String lowered = path;
    lowered.toLowerCase();
    return lowered == MAIN_FIRMWARE;
}

String compactBytes(uint64_t bytes) {
    char buffer[24];
    if (bytes >= 1073741824ULL) {
        snprintf(buffer, sizeof(buffer), "%.1f GB", static_cast<double>(bytes) / 1073741824.0);
    } else if (bytes >= 1048576ULL) {
        snprintf(buffer, sizeof(buffer), "%.1f MB", static_cast<double>(bytes) / 1048576.0);
    } else if (bytes >= 1024ULL) {
        snprintf(buffer, sizeof(buffer), "%.1f KB", static_cast<double>(bytes) / 1024.0);
    } else {
        snprintf(buffer, sizeof(buffer), "%u B", static_cast<unsigned>(bytes));
    }
    return String(buffer);
}

void setActivity(const String &text) {
    activity = text;
    lastDisplay = 0;
    Serial.println(text);
}

void addResponseHeaders() {
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.sendHeader("Referrer-Policy", "no-referrer");
}

bool secureEquals(const String &left, const String &right) {
    if (left.length() != right.length()) return false;
    uint8_t difference = 0;
    for (size_t index = 0; index < left.length(); ++index) {
        difference |= static_cast<uint8_t>(left[index] ^ right[index]);
    }
    return difference == 0;
}

void rotateWebCredentials() {
    char tokenBuffer[33];
    snprintf(tokenBuffer, sizeof(tokenBuffer), "%08X%08X%08X%08X",
             esp_random(), esp_random(), esp_random(), esp_random());
    sessionToken = tokenBuffer;
    snprintf(webPin, sizeof(webPin), "%06u", static_cast<unsigned>(esp_random() % 1000000U));
    failedLoginAttempts = 0;
    loginBlockedUntil = 0;
    lastDisplay = 0;
}

void generateHotspotPassword() {
    constexpr char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    for (size_t index = 0; index < sizeof(apPassword) - 1; ++index) {
        apPassword[index] = alphabet[esp_random() & 31U];
    }
    apPassword[sizeof(apPassword) - 1] = '\0';
}

String requestSessionCookie() {
    const String cookies = server.header("Cookie");
    const String marker = String(SESSION_COOKIE) + "=";
    int start = 0;
    while (start < static_cast<int>(cookies.length())) {
        int end = cookies.indexOf(';', start);
        if (end < 0) end = cookies.length();
        String item = cookies.substring(start, end);
        item.trim();
        if (item.startsWith(marker)) return item.substring(marker.length());
        start = end + 1;
    }
    return "";
}

bool requestAuthenticated() {
    const String headerToken = server.header("X-Sci-Token");
    if (headerToken.length() && secureEquals(headerToken, sessionToken)) return true;
    const String cookieToken = requestSessionCookie();
    return cookieToken.length() && secureEquals(cookieToken, sessionToken);
}

void sendJsonError(int status, const String &message) {
    addResponseHeaders();
    server.send(status, "application/json; charset=utf-8",
                "{\"ok\":false,\"error\":\"" + jsonEscape(message) + "\"}");
}

bool requireAuthentication() {
    if (requestAuthenticated()) return true;
    sendJsonError(401, "Device code required");
    return false;
}

bool mutationAuthorized() {
    return requestAuthenticated();
}

bool requireMutationAuthorization() {
    return requireAuthentication();
}

String mimeTypeFor(const String &path) {
    String lowered = path;
    lowered.toLowerCase();
    if (lowered.endsWith(".html") || lowered.endsWith(".htm")) return "text/html";
    if (lowered.endsWith(".txt") || lowered.endsWith(".log") || lowered.endsWith(".md")) return "text/plain";
    if (lowered.endsWith(".json")) return "application/json";
    if (lowered.endsWith(".jpg") || lowered.endsWith(".jpeg")) return "image/jpeg";
    if (lowered.endsWith(".png")) return "image/png";
    if (lowered.endsWith(".gif")) return "image/gif";
    if (lowered.endsWith(".mp3")) return "audio/mpeg";
    if (lowered.endsWith(".wav")) return "audio/wav";
    if (lowered.endsWith(".mid") || lowered.endsWith(".midi")) return "audio/midi";
    if (lowered.endsWith(".pdf")) return "application/pdf";
    return "application/octet-stream";
}

String urlEncode(const String &value) {
    const char hex[] = "0123456789ABCDEF";
    String encoded;
    encoded.reserve(value.length() * 3);
    for (size_t index = 0; index < value.length(); ++index) {
        const uint8_t byte = static_cast<uint8_t>(value[index]);
        if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' || byte == '.') {
            encoded += static_cast<char>(byte);
        } else {
            encoded += '%';
            encoded += hex[byte >> 4];
            encoded += hex[byte & 0x0F];
        }
    }
    return encoded;
}

void handleIndex() {
    addResponseHeaders();
    server.sendHeader("Content-Security-Policy",
                      "default-src 'self'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; connect-src 'self'");
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleLogin() {
    const uint32_t now = millis();
    if (loginBlockedUntil && static_cast<int32_t>(loginBlockedUntil - now) > 0) {
        const uint32_t remaining = (loginBlockedUntil - now + 999) / 1000;
        server.sendHeader("Retry-After", String(remaining));
        sendJsonError(429, "Too many attempts; wait " + String(remaining) + " seconds");
        return;
    }
    loginBlockedUntil = 0;
    const String suppliedPin = server.hasArg("pin") ? server.arg("pin") : "";
    if (!secureEquals(suppliedPin, String(webPin))) {
        ++failedLoginAttempts;
        if (failedLoginAttempts >= MAX_LOGIN_ATTEMPTS) {
            failedLoginAttempts = 0;
            loginBlockedUntil = now + LOGIN_LOCKOUT_MS;
            setActivity("Login locked 30s");
            server.sendHeader("Retry-After", "30");
            sendJsonError(429, "Too many attempts; wait 30 seconds");
        } else {
            setActivity("Wrong web code");
            sendJsonError(401, "Incorrect device code");
        }
        return;
    }

    failedLoginAttempts = 0;
    setActivity("Web unlocked");
    addResponseHeaders();
    server.sendHeader("Set-Cookie", String(SESSION_COOKIE) + "=" + sessionToken +
                      "; Path=/; HttpOnly; SameSite=Strict");
    server.send(200, "application/json; charset=utf-8",
                "{\"ok\":true,\"token\":\"" + sessionToken + "\"}");
}

void handleLogout() {
    if (!requireAuthentication()) return;
    rotateWebCredentials();
    setActivity("Manager locked");
    addResponseHeaders();
    server.sendHeader("Set-Cookie", String(SESSION_COOKIE) +
                      "=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict");
    server.send(200, "application/json", "{\"ok\":true}");
}

void handleStatus() {
    if (!requireAuthentication()) return;
    const uint8_t clients = WiFi.softAPgetStationNum();
    String body = "{\"ok\":true,\"ssid\":\"" + jsonEscape(apSsid) +
                  "\",\"ip\":\"" + WiFi.softAPIP().toString() +
                  "\",\"clients\":" + String(clients) +
                  ",\"heap\":" + String(ESP.getFreeHeap()) +
                  ",\"sd\":" + String(sdReady ? "true" : "false") + "}";
    addResponseHeaders();
    server.send(200, "application/json; charset=utf-8", body);
}

void handleList() {
    if (!requireAuthentication()) return;
    if (!sdReady) {
        sendJsonError(503, "SD card unavailable");
        return;
    }
    String path;
    if (!normalizePath(server.hasArg("path") ? server.arg("path") : "/", path)) {
        sendJsonError(400, "Invalid path");
        return;
    }
    File directory = SD.open(path, FILE_READ);
    if (!directory || !directory.isDirectory()) {
        if (directory) directory.close();
        sendJsonError(404, "Folder not found");
        return;
    }

    addResponseHeaders();
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/json; charset=utf-8", "");
    server.sendContent("{\"ok\":true,\"path\":\"" + jsonEscape(path) +
                       "\",\"total\":" + uint64Text(SD.totalBytes()) +
                       ",\"used\":" + uint64Text(SD.usedBytes()) + ",\"items\":[");
    bool first = true;
    while (true) {
        File entry = directory.openNextFile(FILE_READ);
        if (!entry) break;
        String entryPath = entry.path();
        String name = baseName(entryPath);
        const bool temporary = name.endsWith(TEMP_SUFFIX) || name.endsWith(BACKUP_SUFFIX);
        if (!temporary) {
            if (!entryPath.startsWith("/")) entryPath = joinPath(path, name);
            String item = first ? "" : ",";
            item += "{\"name\":\"" + jsonEscape(name) +
                    "\",\"path\":\"" + jsonEscape(entryPath) +
                    "\",\"directory\":" + String(entry.isDirectory() ? "true" : "false") +
                    ",\"size\":" + uint64Text(entry.size()) + "}";
            server.sendContent(item);
            first = false;
        }
        entry.close();
        yield();
    }
    directory.close();
    server.sendContent("]}");
    server.sendContent("");
}

void handleDownload() {
    if (!requireAuthentication()) return;
    if (!sdReady) {
        sendJsonError(503, "SD card unavailable");
        return;
    }
    String path;
    if (!server.hasArg("path") || !normalizePath(server.arg("path"), path) || path == "/") {
        sendJsonError(400, "Invalid file path");
        return;
    }
    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) file.close();
        sendJsonError(404, "File not found");
        return;
    }
    setActivity("Download " + baseName(path));
    addResponseHeaders();
    server.sendHeader("Content-Disposition",
                      "attachment; filename*=UTF-8''" + urlEncode(baseName(path)));
    server.streamFile(file, mimeTypeFor(path));
    file.close();
    setActivity("Download complete");
}

void handleMkdir() {
    if (!requireMutationAuthorization()) return;
    if (!sdReady) {
        sendJsonError(503, "SD card unavailable");
        return;
    }
    String path;
    if (!server.hasArg("path") || !normalizePath(server.arg("path"), path) || path == "/") {
        sendJsonError(400, "Invalid folder path");
        return;
    }
    if (SD.exists(path)) {
        sendJsonError(409, "A file or folder already exists there");
        return;
    }
    if (!SD.mkdir(path)) {
        sendJsonError(500, "Could not create folder");
        return;
    }
    setActivity("Created " + baseName(path));
    addResponseHeaders();
    server.send(200, "application/json", "{\"ok\":true}");
}

void handleRename() {
    if (!requireMutationAuthorization()) return;
    if (!sdReady) {
        sendJsonError(503, "SD card unavailable");
        return;
    }
    String from;
    String to;
    if (!server.hasArg("from") || !server.hasArg("to") ||
        !normalizePath(server.arg("from"), from) || !normalizePath(server.arg("to"), to) ||
        from == "/" || to == "/") {
        sendJsonError(400, "Invalid rename path");
        return;
    }
    if (isProtectedPath(from) || isProtectedPath(to)) {
        sendJsonError(403, "main.bin is protected from rename");
        return;
    }
    if (!SD.exists(from)) {
        sendJsonError(404, "Source does not exist");
        return;
    }
    if (SD.exists(to)) {
        sendJsonError(409, "Destination already exists");
        return;
    }
    if (!SD.rename(from, to)) {
        sendJsonError(500, "Rename failed");
        return;
    }
    setActivity("Renamed " + baseName(to));
    addResponseHeaders();
    server.send(200, "application/json", "{\"ok\":true}");
}

bool deleteTree(const String &path, uint8_t depth) {
    if (depth > 16 || isProtectedPath(path)) return false;
    File target = SD.open(path, FILE_READ);
    if (!target) return false;
    if (!target.isDirectory()) {
        target.close();
        return SD.remove(path);
    }
    while (true) {
        File child = target.openNextFile(FILE_READ);
        if (!child) break;
        String childPath = child.path();
        const String childName = baseName(childPath);
        if (!childPath.startsWith("/")) childPath = joinPath(path, childName);
        child.close();
        if (!deleteTree(childPath, depth + 1)) {
            target.close();
            return false;
        }
        yield();
    }
    target.close();
    return SD.rmdir(path);
}

void handleDelete() {
    if (!requireMutationAuthorization()) return;
    if (!sdReady) {
        sendJsonError(503, "SD card unavailable");
        return;
    }
    String path;
    if (!server.hasArg("path") || !normalizePath(server.arg("path"), path) || path == "/") {
        sendJsonError(400, "Invalid delete path");
        return;
    }
    if (isProtectedPath(path)) {
        sendJsonError(403, "main.bin is protected from deletion");
        return;
    }
    if (!SD.exists(path)) {
        sendJsonError(404, "File or folder not found");
        return;
    }
    if (!deleteTree(path, 0)) {
        sendJsonError(500, "Delete failed");
        return;
    }
    setActivity("Deleted " + baseName(path));
    addResponseHeaders();
    server.send(200, "application/json", "{\"ok\":true}");
}

bool commitUpload() {
    const String backupPath = uploadFinalPath + BACKUP_SUFFIX;
    if (SD.exists(backupPath)) SD.remove(backupPath);
    bool hadOriginal = SD.exists(uploadFinalPath);
    if (hadOriginal) {
        File existing = SD.open(uploadFinalPath, FILE_READ);
        const bool isDirectory = existing && existing.isDirectory();
        if (existing) existing.close();
        if (isDirectory || !SD.rename(uploadFinalPath, backupPath)) {
            uploadError = "Could not preserve existing file";
            return false;
        }
    }
    if (!SD.rename(uploadTempPath, uploadFinalPath)) {
        if (hadOriginal) SD.rename(backupPath, uploadFinalPath);
        uploadError = "Could not finalize uploaded file";
        return false;
    }
    if (hadOriginal) SD.remove(backupPath);
    return true;
}

void handleUploadData() {
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        uploadActive = true;
        uploadSucceeded = false;
        uploadAuthorized = mutationAuthorized();
        uploadBytes = 0;
        uploadError = "";
        uploadFinalPath = "";
        uploadTempPath = "";
        if (!uploadAuthorized) {
            uploadError = "Session authorization failed";
            return;
        }
        if (!sdReady) {
            uploadError = "SD card unavailable";
            return;
        }
        String directory;
        String filename;
        if (!normalizePath(server.hasArg("dir") ? server.arg("dir") : "/", directory) ||
            !cleanUploadName(upload.filename, filename)) {
            uploadError = "Invalid upload path";
            return;
        }
        File parent = SD.open(directory, FILE_READ);
        const bool validParent = parent && parent.isDirectory();
        if (parent) parent.close();
        if (!validParent) {
            uploadError = "Upload folder not found";
            return;
        }
        uploadFinalPath = joinPath(directory, filename);
        uploadTempPath = uploadFinalPath + TEMP_SUFFIX;
        if (SD.exists(uploadTempPath)) SD.remove(uploadTempPath);
        uploadFile = SD.open(uploadTempPath, FILE_WRITE);
        if (!uploadFile) {
            uploadError = "Could not create temporary file";
            return;
        }
        setActivity("Upload " + filename);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile && !uploadError.length()) {
            const size_t written = uploadFile.write(upload.buf, upload.currentSize);
            uploadBytes += written;
            if (written != upload.currentSize) uploadError = "SD write failed or card is full";
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.flush();
            uploadFile.close();
        }
        if (!uploadError.length() && uploadBytes != upload.totalSize) {
            uploadError = "Upload ended before all bytes were written";
        }
        if (!uploadError.length()) uploadSucceeded = commitUpload();
        if (!uploadSucceeded && uploadTempPath.length() && SD.exists(uploadTempPath)) {
            SD.remove(uploadTempPath);
        }
        uploadActive = false;
        setActivity(uploadSucceeded ? "Upload complete" : "Upload failed");
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        if (uploadFile) uploadFile.close();
        if (uploadTempPath.length() && SD.exists(uploadTempPath)) SD.remove(uploadTempPath);
        uploadError = "Upload aborted";
        uploadSucceeded = false;
        uploadActive = false;
        setActivity("Upload aborted");
    }
}

void clearUploadResult() {
    uploadAuthorized = false;
    uploadSucceeded = false;
    uploadError = "";
    uploadFinalPath = "";
    uploadTempPath = "";
    uploadBytes = 0;
}

void handleUploadComplete() {
    if (!uploadAuthorized) {
        const String message = uploadError.length() ? uploadError : "Session authorization failed";
        clearUploadResult();
        sendJsonError(401, message);
        return;
    }
    if (!uploadSucceeded) {
        const String message = uploadError.length() ? uploadError : "Upload failed";
        clearUploadResult();
        sendJsonError(500, message);
        return;
    }
    const String path = uploadFinalPath;
    const size_t bytes = uploadBytes;
    clearUploadResult();
    addResponseHeaders();
    server.send(200, "application/json",
                "{\"ok\":true,\"path\":\"" + jsonEscape(path) +
                "\",\"size\":" + uint64Text(bytes) + "}");
}

void handleExitRequest() {
    if (!requireMutationAuthorization()) return;
    if (uploadActive) {
        sendJsonError(409, "Wait for the current upload to finish");
        return;
    }
    if (!sdReady || !SD.exists(MAIN_FIRMWARE)) {
        sendJsonError(409, "main.bin is missing");
        return;
    }
    addResponseHeaders();
    server.send(202, "application/json", "{\"ok\":true}");
    returnAt = millis() + 700;
}

void handleNotFound() {
    if (server.uri().startsWith("/api/")) {
        sendJsonError(404, "API route not found");
    } else if (server.method() == HTTP_GET) {
        handleIndex();
    } else {
        sendJsonError(404, "Not found");
    }
}

void configureServer() {
    const char *headers[] = {"X-Sci-Token", "Cookie"};
    server.collectHeaders(headers, 2);
    server.on("/", HTTP_GET, handleIndex);
    server.on("/generate_204", HTTP_GET, handleIndex);
    server.on("/hotspot-detect.html", HTTP_GET, handleIndex);
    server.on("/connecttest.txt", HTTP_GET, handleIndex);
    server.on("/ncsi.txt", HTTP_GET, handleIndex);
    server.on("/favicon.ico", HTTP_GET, []() { server.send(204); });
    server.on("/api/login", HTTP_POST, handleLogin);
    server.on("/api/logout", HTTP_POST, handleLogout);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/list", HTTP_GET, handleList);
    server.on("/api/download", HTTP_GET, handleDownload);
    server.on("/api/mkdir", HTTP_POST, handleMkdir);
    server.on("/api/rename", HTTP_POST, handleRename);
    server.on("/api/delete", HTTP_POST, handleDelete);
    server.on("/api/upload", HTTP_POST, handleUploadComplete, handleUploadData);
    server.on("/api/exit", HTTP_POST, handleExitRequest);
    server.onNotFound(handleNotFound);
    server.begin();
}

void drawStatus() {
    display.clearBuffer();
    display.setFont(u8g2_font_5x7_tf);
    const String titleLine = "SD WEB MANAGER  C" + String(WiFi.softAPgetStationNum());
    display.drawStr(2, 7, titleLine.c_str());
    const String wifiLine = "SSID " + apSsid;
    display.drawStr(2, 18, wifiLine.c_str());
    const String passwordLine = String("WIFI PASS ") + apPassword;
    display.drawStr(2, 28, passwordLine.c_str());
    display.drawStr(2, 38, "IP 192.168.4.1");
    display.setFont(u8g2_font_6x10_tf);
    const String codeLine = "WEB CODE " + String(webPin);
    display.drawStr(2, 51, codeLine.c_str());
    display.setFont(u8g2_font_5x7_tf);
    String bottom = activity;
    while (bottom.length() && display.getStrWidth(bottom.c_str()) > 177) {
        bottom.remove(bottom.length() - 1);
    }
    display.drawStr(2, 62, bottom.c_str());
    display.drawStr(181, 62, "Hold ESC");
    display.sendBuffer();
}

bool installMainFirmware() {
    if (uploadActive) return false;
    File firmware = SD.open(MAIN_FIRMWARE, FILE_READ);
    if (!firmware || firmware.isDirectory() || !firmware.size()) {
        if (firmware) firmware.close();
        setActivity("Missing main.bin");
        return false;
    }
    const size_t size = firmware.size();
    if (!Update.begin(size, U_FLASH)) {
        firmware.close();
        setActivity("Update begin failed");
        return false;
    }

    server.stop();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(52, 15, "Returning to calculator");
    display.drawFrame(4, 28, 248, 9);
    display.sendBuffer();

    uint8_t buffer[1024];
    size_t written = 0;
    uint8_t previousPercent = 255;
    while (firmware.available()) {
        const size_t count = firmware.read(buffer, sizeof(buffer));
        if (!count || Update.write(buffer, count) != count) break;
        written += count;
        const uint8_t percent = static_cast<uint8_t>(written * 100ULL / size);
        if (percent != previousPercent) {
            previousPercent = percent;
            display.setDrawColor(0);
            display.drawBox(5, 29, 246, 7);
            display.setDrawColor(1);
            display.drawBox(5, 29, percent * 246 / 100, 7);
            display.sendBuffer();
        }
        yield();
    }
    firmware.close();
    if (written == size && Update.end(true) && Update.isFinished()) {
        display.clearBuffer();
        display.drawStr(69, 35, "Calculator restored");
        display.sendBuffer();
        delay(250);
        ESP.restart();
        return true;
    }
    Update.abort();
    display.clearBuffer();
    display.drawStr(72, 28, "Update failed");
    display.drawStr(47, 45, "Restarting SD Manager");
    display.sendBuffer();
    delay(1800);
    ESP.restart();
    return false;
}
}

void setup() {
    Serial.begin(115200);
    pinMode(OLED_CS, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(OLED_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    SPI.begin(18, 19, 23);
    display.begin();
    display.setContrast(220);
    keyboard.begin();

    sdReady = SD.begin(SD_CS, SPI);
    const uint64_t chipId = ESP.getEfuseMac();
    char suffix[5];
    snprintf(suffix, sizeof(suffix), "%04X", static_cast<unsigned>(chipId & 0xFFFF));
    apSsid = "SCI-CALC-" + String(suffix);
    generateHotspotPassword();
    rotateWebCredentials();

    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
    const bool apReady = WiFi.softAP(apSsid.c_str(), apPassword, 6, false, 4);
    if (apReady) {
        dnsServer.start(53, "*", AP_IP);
        configureServer();
        setActivity(sdReady ? "Ready" : "No SD card");
    } else {
        setActivity("WiFi start failed");
    }
    drawStatus();
    Serial.printf("SD Manager SSID=%s IP=%s SD=%s heap=%u\n",
                  apSsid.c_str(), WiFi.softAPIP().toString().c_str(),
                  sdReady ? "ready" : "failed", ESP.getFreeHeap());
}

void loop() {
    dnsServer.processNextRequest();
    server.handleClient();
    keyboard.update();

    if (!uploadActive && keyboard.isDown(ManagerKey::Escape) &&
        keyboard.heldMs(ManagerKey::Escape) >= EXIT_HOLD_MS) {
        if (!exitTriggered) {
            exitTriggered = true;
            installMainFirmware();
        }
    } else if (!keyboard.isDown(ManagerKey::Escape)) {
        exitTriggered = false;
    }

    if (returnAt && static_cast<int32_t>(millis() - returnAt) >= 0) {
        returnAt = 0;
        installMainFirmware();
    }
    if (millis() - lastDisplay >= DISPLAY_INTERVAL_MS) {
        lastDisplay = millis();
        drawStatus();
    }
    delay(1);
}
