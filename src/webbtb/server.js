/*
    I just want to add that the markdown conversion code is garbage.
    It's very unreliable, only converts a part of markdown and markdown that
    Looks fine in VSCode or on GitHub could be converted to crazy stuff with whole sections italicized and invalid links.
*/

const PORT = 8080;
const PORT_HTTPS = 8081;
const stats_path = "stats.json"

const DISABLE_HTTPS = true;

const http = require("http");
const https = require("https");
const fs = require("fs");
const path_module = require('path');


class Statistics {
    requests = 0;
    failed_requests = 0;
    index_requests = 0;
    addresses = {};
    request_paths = {};
}

let stats = new Statistics();

// #################
//  Read stats, setup termination logic, and listen
// #################
try {
    const stat_data = fs.readFileSync(stats_path, "utf8")
    let obj = JSON.parse(stat_data)
    let keys = Object.keys(obj)

    for (let i=0;i<keys.length;i++) {
        stats[keys[i]] = obj[keys[i]]
    }
} catch(err) {
    // stats not found, do nothing
}

process.on('SIGINT', OnTerminate)
process.on('SIGHUP', OnTerminate)
process.on('SIGTERM', OnTerminate)
process.on('SIGBREAK', OnTerminate)

let is_timed = false;

function ScheduleStatSave() {
    if(!is_timed) {
        is_timed = true
        setTimeout(()=>{
            SaveStats()
            is_timed = false;
        }, 1 * 1000)
    }
}
// returns false on failure
function is_path_sanitized(filename) {
    // https://stackoverflow.com/questions/38517506/nodejs-safest-path-and-file-handling

    // 2) Whitelisting
    let dot_count = 0;
    for(let i=0;i<filename.length;i++) {
        let chr = filename[i]
        let code = chr.charCodeAt(0)
        if (code >= "A".charCodeAt(0) && code <= "Z".charCodeAt(0))
            continue
        if (code >= "a".charCodeAt(0) && code <= "z".charCodeAt(0))
            continue
        if (code >= "0".charCodeAt(0) && code <= "9".charCodeAt(0))
            continue
        if(chr == '-' || chr == '_' || chr == '/')
            continue

        if(chr == '.') {
            dot_count++;
            if(dot_count < 2)
                continue;
        }
        // bad character
        // console.log("bad char",filename, i, chr)
        return false
    }
    return true;
}
function requestListener(req, res) {
    var root = "public/";
    let url_split = req.url.split("?")
    let base_part = url_split[0];
    var path = path_module.join("public/", base_part).replace("\\","/");
    
    // console.log(req.url)
    
    if(path.indexOf(root) != 0) {
        // someone is being sneaky
        // console.log("sneaky!", root, path)
        res.end()
        return;
    }
    
    let options = {};
    if(url_split.length>1){
        // TODO: Error handling
        let option_list = url_split[1].split("&") // can & be escaped in options?
        for(let i=0;i<option_list.length;i++) {
            let split = option_list[i].split("=")
            if(split.length == 2) {
                options[split[0]] = split[1];
            } else {
                // console.log("bad option format")
            }
        }
        // console.log("opts:",options)
    }
    
    if(base_part == "/") {
        path += "index.html"
    }
    if(base_part == "/guide") {
        path += ".html"
    }
    // console.log("REQ",path)

    if(stats.request_paths[req.url] == undefined) {
        stats.request_paths[req.url] = 1
    } else {
        stats.request_paths[req.url]++
    }

    if(is_path_sanitized(path)) {
        try {
            const ext = path.substring(path.lastIndexOf(".")+1)
            
            // console.log("Read",path, "ext",ext)
            let data = fs.readFileSync(path);
            let type = "text/html"
            if(ext == "jpg")
                type = "image/jpg"
            if(ext == "mp3")
                type = "audio/mpeg"
            res.writeHead(200, {'Content-Type': type});

            
            if(path == "public/guide.html") {
                data = ModifyContent(data, options) // data is returned without modification if nothing changed
            }

            res.write(data)
            res.end()
            
            try {
                let ip = req.socket.remoteAddress
                let ips = req.headers['x-forwarded-for']
                if(ips != null) {
                    first = ips.split(",")[0].trim()
                    if(first.length > 0)
                        ip = first;
                }
                ip = ip.split(":")[3]
                // console.log("IP:",ip)
                if(stats.addresses[ip] == undefined) {
                    stats.addresses[ip] = 1
                } else {
                    stats.addresses[ip]++
                }
            } catch (err) {
                // do nothing
            }
            if(path == "public/index.html") {
                stats.index_requests++
            }
            stats.requests++

            ScheduleStatSave()
            return
        } catch (err) {
            if (err.errno == -2) {
                console.log("File '" + path + "' not found")
            } else {
                console.error(err);
            }
        }
    } else {
        console.log("Suspicious request '"+path+"'")
        // we don't respond to suspicious requests
        res.end()
        return
    }
    res.writeHead(404)
    res.end()
    stats.failed_requests++
    ScheduleStatSave()
    return
};

// #################
//      HTTP
// #################
const server = http.createServer(requestListener);
server.listen(PORT, () => {
    console.log("Server is running on", PORT);
});

// #################
//      HTTPS
// #################
/*
https://www.youtube.com/watch?v=USrMdBF0zc
https://support.hostinger.com/en/articles/6865487-how-to-install-ssl-on-vps-using-certbot
openssl genrsa -out security/client-key.pem 2048
openssl req -new -key security/client-key.pem -out security/client.csr
openssl x509 -req -in security/client.csr -signkey security/client-key.pem -out security/client-cert.pem
chmod +r security/client-key.pem security/client-cert.pem
*/
if (!DISABLE_HTTPS) {
    const options = {
        // From openssl
        // key: fs.readFileSync('security/client-key.pem'),
        // cert: fs.readFileSync('security/client-cert.pem'),
        // From certbot
        key: fs.readFileSync('security/privkey.pem'),
        cert: fs.readFileSync('security/fullchain.pem'),
    };
    const https_server = https.createServer(options, requestListener);

    https_server.listen(PORT_HTTPS, () => {
        console.log("Server is running on", PORT_HTTPS);
    });
}

function OnTerminate() {
    SaveStats()
    process.exit()
}
function SaveStats() {
    let data = JSON.stringify(stats)
    fs.writeFileSync(stats_path, data)
}

function ModifyContent(data, options) {
    let string = data.toString()
    let keyword, index_of_insert, text = "";

    let md_dir = "../../docs/guide"
    // TODO: Error handling
    let files = fs.readdirSync(md_dir)
    files.sort((a,b)=>{
        // TODO: Handle error
        let n0 = parseInt(a.split("-")[0])
        let n1 = parseInt(b.split("-")[0])
        // console.log(a,b,n0,n1)
        return n0 - n1
    })

    // console.log(options)

    // TODO: Sanitize options, you can do script injection thing otherwise

    let md_title = "no title"
    let title_was_set = false;
    if(options["md"] != undefined) {
        md_title = options["md"]
        title_was_set = true
    } else if (files.length > 0) {
        // should be 00-introdution or the alphbetically first file
        // i don't think readdirSync is guarranteed to return that
        md_title = files[0];
        title_was_set = true
    }

    function fix() {
        const pre = string.substring(0, index_of_insert)
        const post = string.substring(index_of_insert + keyword.length)
        // TODO: We should not set string like this because the text we
        //   insert may contain "insert" keywords (INSERT_MD_TITLE). 
        //   Probably won't happen but what if we create keywords in the
        //   future that might exist.

        string = pre + text + post
        text = ""
    }

    // TODO: Sanitize replaced markdown, it may contain script tags which we shouldn't execute.
    
    if((index_of_insert = string.indexOf(keyword = "INSERT_MD_FILES")) != -1) {
        for (let i=0;i<files.length;i++) {
            text += "<a href='./guide?md="+files[i]+"'>" + files[i] + "</a><br>"
        }
        fix()
    }
    if((index_of_insert = string.indexOf(keyword = "INSERT_MD_TITLE")) != -1) {
        let tmp = md_title.replaceAll("%20"," ")
        text += tmp
        fix()
    }
    if((index_of_insert = string.indexOf(keyword = "INSERT_MD_CONTENT")) != -1) {
        if(title_was_set) {
            let tmp = md_title.replaceAll("%20"," ")
            let path = md_dir + "/" + tmp
            // TODO: Sanitize path, otherwise user can access any file
            try {
                let md_data = fs.readFileSync(path)
                let html_data = ConvertMDToHTML(md_data)
                text += html_data
                fix()
            } catch (e) {
                console.error(e)
            }
        }
    }



    return string
    // const pre = string.substring(0, index_of_insert)
    // const post = string.substring(index_of_insert + keyword.length)
    // return pre + text + post
}

// Code to manually test conversions
// console.log(ConvertMDToHTML("well\n- A\n- B\nMore text"))
// console.log(ConvertMDToHTML("yes<!-- no -->yes"))
// console.log(ConvertMDToHTML(" also the *Slice* type which "))
// console.log(ConvertMDToHTML("well [link](hey.png) hum"))
// console.log(ConvertMDToHTML("Text is\nvery close"))
// console.log(ConvertMDToHTML("but this is\n\nseparated"))
// console.log(ConvertMDToHTML("This is just.  \nA new line"))
// console.log(ConvertMDToHTML("no \n```\nyes\ndude!\n```\n haha"))
// console.log(ConvertMDToHTML("okay *lier* bro"))
// console.log(ConvertMDToHTML("yes\n# okay dude\nhey bro"))

function ConvertMDPathToWebPath(path) {
    if(path.length < 2)
        return path;

    let at = path.indexOf("/docs/guide/")
    if(at == 0) {
        return "/guide?md="+path.substring("/docs/guide/".length)
    }
    if(path[0] == "/") {
        // TODO: Do not hardcode the path to the 'dev' branch.
        //   Ideally, it would be hardcoded to the current commit
        //   the website has access to, that way the files will be properly synchronized.
        //   I don't know how to do this so it might be fine to use the default branch which is dev at the moment.
        //   (should be changed because dev won't be the default branch in the future)
        return "https://github.com/Emarioo/BetterThanBatch/tree/dev/" + path.
        substring(1);
    }
    return path
}

// takes in string, returns string
// Converts markdown to html
function ConvertMDToHTML(data) {
    data = data.toString()
    let text = "";

    let head = 0;
    const NO_STYLE = 0
    const BOLD = 1
    const ITALIC = 2
    let is_styled = NO_STYLE;
    let header_level = 0;
    let inside_header = false;
    let inside_paragraph = false;
    let inside_list = false;
    let inside_code_block = false;
    let inside_tiny_code = false;

    let style_stack = []

    function is_whitespace(c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' // carriage return to prevent unforseen problems
    }
    function is_alphanum(c) {
        let n = c.charCodeAt(0)
        return (n >= 'A'.charCodeAt(0) && n <= 'Z'.charCodeAt(0))
            || (n >= 'a'.charCodeAt(0) && n <= 'z'.charCodeAt(0))
            || (n >= '0'.charCodeAt(0) && n <= '9'.charCodeAt(0))
            || c == '_'
    }

    // TODO: Parsing assumes that the markdown is syntactically correct.
    //   If it's not then odd stuff will be printed.

    while (head < data.length) {
        let chr = data[head];
        let chr_prev = '\0'
        let chr_next = '\0'
        let chr_next2 = '\0'
        let chr_next3 = '\0'
        if (head-1 >= 0)
            chr_prev = data[head-1]
        if (head+1 < data.length)
            chr_next = data[head+1]
        if (head+2 < data.length)
            chr_next2 = data[head+2]
        if (head+3 < data.length)
            chr_next3 = data[head+3]
        head++

        // TODO: Code block conversion does not follow the standard for edge cases. We may not care though.
        if(chr == '`' && chr_next == '`' && chr_next2 == '`') {
            // TODO: Parse language hint and apply syntax highlighting
            head += 2
            if(!inside_code_block) {
                text += "<div class='code_div'><code>"
            } else {
                text += "</code></div>"
            }
            inside_code_block = !inside_code_block
            continue
        } else if (chr == '`') {
            if(!inside_tiny_code) {
                text += "<tiny_code>"
            } else {
                text += "</tiny_code>"
            }
            inside_tiny_code = !inside_tiny_code
            continue
        }

        if(inside_code_block || inside_tiny_code) {
            // TODO: Apply syntax highlighting. For this we should append characters to a temporary string.
            //   Apply highlighting to that string (wrap words in <span> with colors) then convert arrows to &lt/&gt
            //   and finally add it to 'text'
            if(chr == '<')
                text += "&lt;"
            else if(chr == '>')
                text += "&gt;"
            else
                text += chr
            continue
        }
        if(chr == '<' && chr_next=='!'&&chr_next2=='-'&&chr_next3=='-') {
            // skip comment
            head += 3;
            // TODO: This is BUGGY, most of the code is buggy actually because
            //   it doesn't handle end of file... it won't crash because 'head<data.length'
            //   but it won't emit correct text either
            
            while(head < data.length) {
                let tmp = data[head]
                if(tmp == '-' && head + 2 < data.length && data[head+1] == '-' && data[head+2] == '>') {
                    head+=3
                    break
                }
                head++
            }
            continue;
        }

        if(chr == '\\' && head < data.length) {
            // skip backwards slash
            // always print character after
            text += data[head]
            head++
            continue
        }

        if(chr == '#') {
            // Assert(header_level == 0)
            header_level = 1;
            
            // calculate header level
            while (head < data.length) {
                let tmp = data[head];
                if (tmp != '#') {
                    break
                }
                head++;
                header_level++;
            }
            // trim white space after hashtags
            while (head < data.length) {
                let tmp = data[head];
                if (tmp != ' ' && tmp != '\t') {
                    break
                }
                head++;
            }

            inside_header = true;
            text += "<h"+header_level+">";
            continue;
        }

        if(!inside_paragraph && !inside_header && !is_whitespace(chr)) {
            inside_paragraph = true;
            text += "<p>";
        }
        
        // TODO: Styling is not handled according to the standard. We need to fix this. But perhaps we don't care.
        // TODO: Markdown that write about pointer types such as 'char*' will cause problems with italics in certain cases. It was worse but i fixed it cheaply by checking for trailing alphanumeric.
        if((chr == '*' && (chr_next == '*' || (is_alphanum(chr_next) || style_stack.length > 0))) || (is_whitespace(chr_prev) && is_whitespace(chr_next) && chr == '_')) {
            let style_level = 1;
            
            // calculate style level
            while (head < data.length) {
                let tmp = data[head];
                if (tmp != '*' && tmp != '_') {
                    break
                }
                head++;
                style_level++;
            }
            let last_style = 0
            if(style_stack.length > 0
                && (last_style = style_stack[style_stack.length - 1])
                && last_style == style_level) {
                style_stack.pop()
                text += "</span>"
            } else {
                style_stack.push(style_level)
                text += "<span style='"
                if(style_level >= 2)
                    text += "font-weight:bold;"
                if (style_level % 2 == 1)
                    text += "font-style:italic;"
                
                text += "'>"
            }
            continue
        }
        // console.log(chr, inside_paragraph, inside_header, chr_next)
        // if(!inside_header && !inside_paragraph && chr == '-' && chr_next == ' ') {
        //     console.log("hi yoo dude")
        //     head++; // skip space
        //     inside_list = true;
        //     text += "<ul>"
        //     text += "<li>"
        //     continue;
        // }
        
        if(inside_list) {
            if(chr == '\n') {
                text += "</li>"
                // newline indicates end of list item, however, we can't emit </ul> yet
                // because the following line may be another list item. Those items needs to exist
                // within the same <ul>. We could just emit <li> but they don't have the proper indentation (can be fixed with CSS, margin-left)
                // and there may be other problems.
                let found = false;
                while(head < data.length) {
                    let tmp = data[head];
                    let tmp2 = '\0'
                    if(head+1 < data.length)
                        tmp2 = data[head + 1];
                        
                    if(tmp == ' ' || tmp == '\t') { // skip whitespace except for new line
                        head++
                        continue
                    }
                    
                    if(tmp == '-' && tmp2 == ' ') {
                        head+=2
                        found = true;
                    }
                    break;
                }
                if(found) {
                    text += "<li>"
                    continue;
                }
                
                inside_list = false;
                text += "</ul>"
                continue;   
            }
            text += chr;
            continue;
        } else if (inside_header) {
            if(chr == '\n') {
                text += "</h"+header_level+">\n";
                inside_header = false;
                header_level = 0
            } else {
                text += chr
            }
        } else if (inside_paragraph) {
            if(chr == '\n') {
                let lookahead = head
                let found = false;
                while (lookahead < data.length) {
                    let tmp = data[lookahead];
                    if (!is_whitespace(tmp)) {
                        break
                    }
                    lookahead++;
                    if(tmp == '\n') {
                        found = true;
                        break;
                    }
                }
                if (found) {
                    inside_paragraph = false;
                    text += "</p>";
                    continue
                } else {

                }
            }
            if(chr == '-' && chr_next == ' ') {
                head++; // skip space
                inside_list = true;
                text += "<ul>"
                text += "<li>"
                continue;
            }
            if(chr == '[') {
                // links
                // TODO: Handle exclamation mark
                let start = head
                while (head < data.length) {
                    let tmp = data[head];
                    head++;
                    if(tmp == ']') {
                        break
                    }
                }
                let displayed = data.substring(start, head-1)
                if(data[head] != '(') {
                    // no parenthesis, don't do link
                    // this is flawed, italic, bold is not handled
                    text += "[" + displayed + "]"
                    continue
                }
                head++ // skip open parenthesis
                start = head
                while (head < data.length) {
                    let tmp = data[head];
                    head++;
                    if(tmp == ')') {
                        break
                    }
                }
                let path = data.substring(start, head-1)
                let real_path = ConvertMDPathToWebPath(path)
                // TODO: How to we convert a path described in markdown to a
                //   valid link on the website?
                //   Convert it to a github link unless it points to a another markdown
                //  in docs?
                text += "<a href='"+real_path+"'>";
                text += displayed
                text += "</a>"
                continue;
            }
            text += chr
        } else {
            // UNREACHABLE
        }
    }
    if(inside_paragraph)
        text += "</p>";
    // headers
    // links
    // bold, italic
    // code blocks
    // tiny code blocks
    // lists, check boxes
    return text
}