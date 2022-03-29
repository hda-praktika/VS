class Application {
    constructor() {
        this._totalProductionPower = 0;
        this._totalConsumptionPower = 0;
        this._map = document.getElementById('map');
        this._dots = new Map();

        this._openSocket();
        this._initChart();
    }

    _openSocket() {
        this._socket = new WebSocket(`ws://${location.host}/ws`);
        this._socket.addEventListener('message', message => {
            try {
                const prosumers = JSON.parse(message.data);
                this._processMessage(prosumers);
            } catch (err) {
                console.error(err);
            }
        });
        this._socket.addEventListener('close', () => {
            setTimeout(() => this._openSocket(), 100);
        });
    }

    _initChart() {
        const history = 50;
        const now = Date.now();
        const initArray = (initialValue) => {
            const arr = new Array(history).fill(null);
            arr[0] = {
                x: now - (1000 * history),
                y: initialValue
            };
            for (let i = 1; i < arr.length; ++i) {
                arr[i] = { x: arr[i - 1].x + 1000, y: initialValue };
            }
            return arr;
        };
        this._production = initArray(this._totalProductionPower);
        this._consumption = initArray(this._totalConsumptionPower);

        const updateInterval = 1000;
        setInterval(() => this._updateChart(), updateInterval);

        this._chart = new ApexCharts(document.getElementById("energy"), {
            series: [{
                name: 'Production',
                data: this._production.slice()
            }, {
                name: 'Consumption',
                data: this._consumption.slice()
            }],
            chart: {
                id: 'realtime',
                height: 400,
                type: 'line',
                animations: {
                    enabled: true,
                    easing: 'linear',
                    animateGradually: {
                        enabled: false
                    },
                    dynamicAnimation: {
                        speed: 1000
                    }
                },
                toolbar: {
                    show: false
                },
                zoom: {
                    enabled: false
                }
            },
            dataLabels: {
                enabled: false
            },
            stroke: {
                curve: 'straight'
            },
            markers: {
                size: 0
            },
            theme: {
                mode: 'dark',
            },
            xaxis: {
                type: 'datetime',
                range: history * 1000
            },
            yaxis: {
                min: 0,
                //max: 10000
            },
        });
        this._chart.render();
    }

    _processMessage(prosumers) {
        const allNotifications = Object.entries(prosumers).map(([_, notification]) => notification);
        this._totalProductionPower = allNotifications.filter(notif => notif.type === 0).reduce((acc, cur) => acc + cur.power, 0);
        this._totalConsumptionPower = allNotifications.filter(notif => notif.type === 1).reduce((acc, cur) => acc + cur.power, 0);

        const prosumerIDs = Object.keys(prosumers);
        for(const prosumerID of prosumerIDs) {
            if(!this._dots.has(prosumerID)) {
                this._dots.set(prosumerID, new MapDot(this._map));
            }

            const dot = this._dots.get(prosumerID);
            dot.setPosition(prosumers[prosumerID].pos_x, prosumers[prosumerID].pos_y);
        }
        for(const prosumerID of this._dots.keys()) {
            // Prüfe, ob sich ein Prosumer verabschiedet hat.
            if(!prosumerIDs.includes(prosumerID)) {
                const dot = this._dots.get(prosumerID);
                dot.delete();
                this._dots.delete(prosumerID);
            }
        }
    }

    _updateChart() {
        this._consumption.shift();
        this._production.shift();

        const now = Date.now();
        this._consumption.push({
            x: now,
            y: this._totalConsumptionPower
        });
        this._production.push({
            x: now,
            y: this._totalProductionPower
        });

        this._chart.updateSeries([{
            data: this._production.slice()
        }, {
            data: this._consumption.slice()
        }]);
    }
};

class MapDot {
    constructor(map) {
        const dot = document.createElement('div');
        dot.classList.add('dot');
        const dotInner = document.createElement('div');
        dot.append(dotInner);
        map.prepend(dot);
        this._dot = dot;
    }

    setPosition(x, y) {
        const toPercentage = (val) => `${Math.max(Math.min(100*val, 100), 0)}%`;
        this._dot.style.top = toPercentage(y);
        this._dot.style.left = toPercentage(x);
    }

    delete() {
        this._dot.parentNode.removeChild(this._dot);
        this._dot = null;
    }
};

new Application;
