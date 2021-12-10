powerMonitor Application
==

**Purpose:** Use a Particle Boron device to monitor the AC power at a remote location.

Use the Particle [web IDE](https://build.particle.io/build) or install the [Particle CLI](https://docs.particle.io/tutorials/developer-tools/cli/) on a workstation

Upload powerMonitor.iso and powerMonitorConfig.h to the web IDE or compile and flash using the CLI:
<pre>$ particle compile boron
$ particle flash <device-name></pre>

Deploy flows.json to a public Node-Red instance (see Notes.)   Configure the SMTP server used to send e-mails in the "change" node of the flow.

Create a Particle web hook to handle the events coming from this application:

* Event Name: sendMail
* Full URL: The URL of your Node-RED flow
* Request Type: POST
* Request Format:  JSON
* JSON:
<ul><pre>{
  "{{{0.key}}}": "{{{0.value}}}",
  "{{{1.key}}}": "{{{1.value}}}"
}</pre></ul>

Node-RED Notes
--
I have deployed to the Node-RED instance on my "webserver"

I also tested deploying to Google's "Cloud Run" environment.  This allows deploying a web service to the cloud in such a manner that you are only charged when the web service actually executes, i.e., when something calls the service.  You are not charged when the service is just sitting, waiting.  For this service, the changes should be almost nil, as it only gets called when the power is out at the facility being monitored.

Cloud Run requires that you build a Docker container that contains your web service.  That container is then deployed to the Cloud Run environment where it runs when called.  Here's how I built that container:

Deploy an empty Node-RED container locally, binding the Node-RED container's data directory to a local directory  
  
    $ mkdir -m 777 -p ~/src/powerMonitor/nodered.docker/data
    $ docker run --rm -p 1880:1880 -v ~/src/powerMonitor/nodered.docker/data:/data --name node-red nodered/node-red:latest</pre>

Import the flows.json from this project into that instance of Node-RED.

Configure the "changes" node in the flow so that it can send email via the target SMTP server.

Test the local Node-RED flow:

    $ curl --header "Content-Type: application/json" -d '{"address":"email@address.one","message":"testing the flow"}' http://localhost:1880/sendMail</pre>

Stop the local Docker container.

Build a "sendMailService" Docker container image using the Dockerfile in the nodered.docker directory of this project.  It will include the flows.json file that was created in the previous step. Adjust the "COPY" step in the Dockerfile if necessary, to copy from the correct local directory.

    $ docker build -t sendmailservice .

Test this container locally to be sure that it works

    $ docker run --rm -p 1880:1880 --name sendmailservice sendmailservice
    $ curl --header "Content-Type: application/json" -d '{"address":"email@address.one","message":"testing the flow"}' http://localhost:1880/sendMail
    $ docker stop sendmailservice</pre>

Deploy and run this container image in the Cloud Run environment.
