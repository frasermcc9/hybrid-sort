const chokidar = require('chokidar');
const { exec } = require("child_process");

console.log("Watching...")

chokidar.watch("**/*.c", { usePolling: true }).on('all', (event, path) => {
    exec(`cc -o2 ${path} -o ${path.slice(0, path.length - 2)} -lm -lpthread`, (err, stdout, stderr) => {
        if (err) console.log(err)
        if (stderr) console.log(stderr)
        if (stdout) console.log(stdout)
        exec(`${path.slice(0, path.length - 2)} 6`, (err, stdout, stderr) => {
            if (err) console.log(err)
            if (stderr) console.log(stderr)
            if (stdout) console.log(stdout)
        })
    });
    console.log(event, path);
});