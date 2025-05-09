
window.onload = function() {
    insert_latest_release()
    
    // TODO: Temporary
    /*
    const wipMessage = document.querySelector('.work-in-progress');

    setTimeout(() => {
        wipMessage.classList.add('hide-wip');
    }, 7000);
    */
}

function componentToHex(c) {
    const hex = c.toString(16);
    return hex.length == 1 ? "0" + hex : hex;
  }
  
function rgbToHex(r, g, b, a) {
    return "#" + componentToHex(r) + componentToHex(g) + componentToHex(b) + componentToHex(a);
}

function rand_rgb() {
    function rand(min, max) {
        return Math.floor((Math.random() * (max-min) + min) * 255)
    }
    const min = alpha_min
    const max = alpha_max
    return rgbToHex(rand(min, max),rand(min, max),rand(min, max), rand(alpha_min,alpha_max));
}

async function insert_latest_release() {
    let download = document.getElementsByClassName("cto_download")[0]
    if(!download.innerHTML.includes("Github")) {
        // download button has Download BTB text which means it's ready.
        return
    }
    console.log(download.innerHTML)

    let failed_fetch_msg = "<b>Github Releases</b><br>(could not fetch latest release)"

    let data = null
    try {
        let res = await fetch("/api/latest_release")
        if (!res.ok) {
            console.log(error)
            download.innerHTML = failed_fetch_msg
            return
        }
        data = await res.text()
        data = JSON.parse(data)
    } catch(error) {
        console.log("ERROR: Could not fetch information for latest release", error)
        download.innerHTML = failed_fetch_msg
        return
    }
    // console.log(data)

    if (data.version.length > 1 && data.version[0] == 'v' && parseInt(data.version[1]) != NaN)
        data.version = data.version.substring(1)

    let home_buttons = document.getElementsByClassName("home-buttons")[0]
    let home_left = document.getElementsByClassName("home-left")[0]

    let parser = new DOMParser()
    let version = parser.parseFromString('<p>Latest version: <a target="_blank" href="'+data.url+'"><b>'+data.version+'</b></a> ('+data.date+')</p>', "text/html").body.firstChild

    function find_os_version(name) {
        for (let i=0;i<data.downloads.length;i++) {
            if(data.downloads[i].includes(name))
                return data.downloads[i] 
        }
        return null
    }

    let os = navigator.platform
    let url = null
    if (os.toLowerCase().includes("win")) {
        url = find_os_version("win")
    }
    if (os.toLowerCase().includes("linux")) {
        url = find_os_version("linux")
    }
    if (os.toLowerCase().includes("mac")) {
        url = find_os_version("mac")
    }
    if (url) {
        download.innerHTML = "<b>Download BTB</b>" + os
        download.href = url
    } else {
        download.innerHTML = "<b>Github Releases</b> <br> (not available on "+os+")"
    }
    // console.log(download)
    // console.log(version)
    // home_buttons.appendChild(download);
    home_buttons.insertBefore(download, home_buttons.children[home_buttons.children.length-1]);
    home_left.insertBefore(version, home_left.children[home_left.children.length-2]);
}