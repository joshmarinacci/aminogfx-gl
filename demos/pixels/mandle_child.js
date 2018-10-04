'use strict';

function lerp(a, b, t) {
    return a + t * (b - a);
}

process.on('message', function (m) {
    const row = [];

    for (let i = 0; i < m.iw; i++) {
        const x0 = lerp(m.x0, m.x1, i / m.iw);
        const y0 = m.y;
        let x = 0.0;
        let y = 0.0;
        let iteration = 0;
        const max_iteration = m.iter;

        while (x * x + y * y < 2 * 2 && iteration < max_iteration) {
            const xtemp = x * x - y * y + x0;

            y = 2 * x * y + y0;
            x = xtemp;
            iteration = iteration + 1;
        }

        //return iteration;
        row[i] = iteration;

        //row.push(iteration);
        //console.log("iter = ", iteration);
        //process.send({iter:iteration, ix: m.ix, iy:m.iy});
    }

    process.send({
        row: row,
        iw: m.iw,
        iy: m.iy
    });
});
