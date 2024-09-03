/*
    Useful code, may not be used.
    Remove later if it's not.
*/

var canvas;
var ctx;
var part_count = 300
var part_transform = null;
var part_color = null;
const transform_stride = 4;

var mx=0, my=0;

const part_radius = 2.5;
const color_min = 0.2;
const color_max = 0.9;
const alpha_min = 0.4;
const alpha_max = 0.9;

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

function init_particles() {
    canvas = document.getElementById('particle-canvas');
    ctx = canvas.getContext('2d');
    canvas.width = window.innerWidth;
    canvas.height = window.innerHeight;

    part_transform = new Float32Array(4 * part_count)
    part_color = new Array(part_count);

    for (var i = 0; i < part_count; i++) {
        part_transform[i * transform_stride + 0] = Math.random() * canvas.width,
        part_transform[i * transform_stride + 1] = Math.random() * canvas.height,
        part_transform[i * transform_stride + 2] = Math.random() - 0.5,
        part_transform[i * transform_stride + 3] = Math.random() - 0.5
        part_color[i] = rand_rgb()
    }
    part_color[0] = "#FF0000FF"

    mx = canvas.width/2
    my = canvas.height/2
    addEventListener("mousemove", (e)=>{
        mx = e.clientX
        my = e.clientY
        // console.log(e)
    })

    animate();
}

function animate() {
    canvas.width = window.innerWidth
    canvas.height = window.innerHeight
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    for (var i = 0; i < part_count; i++) {
        let x = part_transform[i * transform_stride + 0]
        let y = part_transform[i * transform_stride + 1]

        let force_x = 0
        let force_y = 0
        

        // let dx = canvas.width/2 - x
        // force_x += -dx / canvas.width * 2
        
        const mouse_dist = 200
        const max_speed = 20
        
        let dx = mx - x
        let dy = my - y
        let dist2 = dx*dx + dy*dy
        // if(i==0)
        //     console.log(Math.sqrt(dist2), mouse_dist*mouse_dist)

        if(dist2 < mouse_dist*mouse_dist) {
            force_x += -mouse_dist/dx * 0.1
            force_x = Math.min(force_x, max_speed)
            force_y += -mouse_dist/dy * 0.1
            force_y = Math.min(force_y, max_speed)
        }

        let prev_x = part_transform[i * transform_stride + 2]
        let prev_y = part_transform[i * transform_stride + 3]
        part_transform[i * transform_stride + 2] += force_x * 0.016
        part_transform[i * transform_stride + 3] += force_y * 0.016

        // if(Math.abs(part_transform[i * transform_stride + 2]) > Math.abs(prev_x) && Math.abs(part_transform[i * transform_stride + 2]) > max_speed) {
        //     part_transform[i * transform_stride + 2] = prev_x;
        // }
        // if(Math.abs(part_transform[i * transform_stride + 3]) > Math.abs(prev_y) && Math.abs(part_transform[i * transform_stride + 3]) > max_speed) {
        //     part_transform[i * transform_stride + 3] = prev_y;
        // }

        part_transform[i * transform_stride + 0] += part_transform[i * transform_stride + 2];
        part_transform[i * transform_stride + 1] += part_transform[i * transform_stride + 3];
        
        // part_transform[i * transform_stride + 2] *= 0.99
        // part_transform[i * transform_stride + 3] *= 0.99


        if (x > canvas.width)   part_transform[i * transform_stride + 0] = 0;
        if (x < 0)              part_transform[i * transform_stride + 0] = canvas.width;
        if (y > canvas.height)  part_transform[i * transform_stride + 1] = 0;
        if (y < 0)              part_transform[i * transform_stride + 1] = canvas.height;
        // if (x > canvas.width || x < 0)   part_transform[i * transform_stride + 2] *= -1;
        // if (y > canvas.height || y < 0)  part_transform[i * transform_stride + 3] *= -1;

        ctx.beginPath();
        ctx.arc(x, y, part_radius, 0, 2 * Math.PI);
        // ctx.fillStyle = 'rgba(255, 255, 255, 0.5)';
        // ctx.fillStyle = 'rgba(200, 200, 200, 0.5)';
        ctx.fillStyle = part_color[i];
        ctx.fill();
    }

    requestAnimationFrame(animate);
}