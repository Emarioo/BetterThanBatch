/*
    Useful code, may not be used.
    Remove later if it's not.
*/


function update_download_link() {
    let link = document.getElementsByClassName("cto_download")[0]
    let os = navigator.platform.toLowerCase()
    let url = "https://github.com/Emarioo/BetterThanBatch/releases/download/v0.2.0"
    if (os.includes("win")) {
        link.href = url + "/btb-0.2.0-win_x64.zip"
        link.target = ""
    }
    if (os.includes("linux")) {
        link.href = url + "/btb-0.2.0-linux_x64.tar.gz"
        link.target = ""
    }
    if (os.includes("mac")) {
        link.innerHTML = "Download BTB <br> (not available on macOS)"
        link.style = "text-align: center"
    }
}

window.onload = () => {
    update_download_link()
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

