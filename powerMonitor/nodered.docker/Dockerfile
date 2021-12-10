FROM nodered/node-red

COPY data/package.json /data/package.json
RUN cd /data && npm install --unsafe-perm --no-update-notifier --no-fund --only=production
COPY data/settings.js /data/settings.js
COPY data/flows.json /data/flows.json
