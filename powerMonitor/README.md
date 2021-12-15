powerMonitor Application
==

**Purpose:** Use a [Particle Boron](https://store.particle.io/products/boron-lte) device to monitor the AC power at a remote location.  Send e-mail notifications on power outages.  Addressing e-mail messages to a [carrier's SMS gateway](https://smsemailgateway.com) allows this application to send text messages as well.

Use the Particle [web IDE](https://build.particle.io/build) or install the [Particle CLI](https://docs.particle.io/tutorials/developer-tools/cli/) on a workstation

Edit powerMonitor.h to set the e-mail addresses that will recieve alerts and the device IDs and names of the devices that will do the power monitoring.

Upload powerMonitor.iso and powerMonitorConfig.h to the web IDE to compile and flash from there, or compile and flash on your local machine using the CLI:
<pre>$ particle compile boron
$ particle flash <i>device-name</i></pre>

Deploy flows.json to a public Node-Red instance (see section below.)   Configure the "email" and "sms messages" nodes of the flow for the SMTP server used to send e-mails.

FYI: The flow is designed to handle messages destined for traditional email, as well as messages going to email-to-text gateways.  Emails need a "Subject" line, texts do not.  When the target email address is all digits before the "@" symbol, the powerMonitor.ino application will send a request to the Node-RED flow that has an empty subject line.  When the flow sees that request, it sends the message through its "sms messages" node.  Every other request goes through the "email" node.

Create a Particle web hook to handle the events coming from this application, forwarding them to the Node-RED service:

* Event Name: sendMail
* Full URL: The URL of your Node-RED flow
* Request Type: POST
* Request Format:  JSON
* JSON:
<ul><pre>{
  "{{{0.key}}}": "{{{0.value}}}",
  "{{{1.key}}}": "{{{1.value}}}",
  "{{{2.key}}}": "{{{2.value}}}"  
}</pre></ul>

Node-RED Notes
--
If you already have a public-facing Node-RED instance somewhere, deply the flow this application uses there.

I deploying the flow to Google's "Cloud Run" environment.  This allows deploying a web service to the cloud in such a manner that you are only charged when the web service actually executes, i.e., when something calls the service.  You are not charged when the service is just sitting, waiting.  For the Node-RED service used with this application the charges should be almost nil, as it only gets called when the power is out at the facility being monitored.

Cloud Run requires that you build a Docker container that contains your web service.  That container is then deployed to the Cloud Run environment where it runs when the web service is called.  Here's how I built my container:

Deploy an empty Node-RED container locally, binding the Node-RED container's data directory to a local directory:
<pre>  
$ mkdir -p ~/src/powerMonitor/nodered.docker/data
$ docker run --rm -p 1880:1880 -v ~/src/powerMonitor/nodered.docker/data:/data --name node-red nodered/node-red:latest
</pre>

Install the [simple-sendmail node](https://flows.nodered.org/node/node-red-contrib-simple-sendmail) into the Node-RED palette.  Import the flows.json file from this project into Node-RED, configure the flow and test it:

Configure the "email" and "sms messages" nodes in the flow so that it can send email via whatever SMTP server you normally use.  (SMTP servers are beyond the scope of this README.  If you're a Google mail user, as an example, then you could read [this](https://support.google.com/a/answer/176600?hl=en))

Test the local Node-RED flow:
<pre>
$ curl --header "Content-Type: application/json" -d '{"address":"<i>phonenumber@sms.gateway.hostname.here</i>","subject":"","message":"testing the flow"}' http://localhost:1880/sendMail
$ curl --header "Content-Type: application/json" -d '{"address":"<i>email@address.here</i>","subject":"Test","message":"testing the flow"}' http://localhost:1880/sendMail
</pre>

Stop the local Docker container.

There is no persistance store associated with containers that run in Cloud Run, so we will turn off the Node-RED admin/edit UI altogether.  Edit the settings.js file in the data directory and set <code>httpAdminRoot: false,</code>.

Build a "sendMailService" Docker container image using the Dockerfile in the nodered.docker directory of this project.  It will include the flows.json, package.json, and settings.js files that were created in the previous step. Adjust the "COPY" statement in the Dockerfile if necessary, to copy from the correct local directory.
<pre>
$ docker build -t sendmailservice .
</pre>

Test this container locally to be sure that it works:
<pre>
$ docker run --rm -p 1880:1880 --name sendmailservice sendmailservice
$ curl --header "Content-Type: application/json" -d '{"address":"<i>phonenumber@sms.gateway.hostname.here</i>","subject":"","message":"testing the flow"}' http://localhost:1880/sendMail
$ curl --header "Content-Type: application/json" -d '{"address":"<i>email@address.here</i>","subject":"Test","message":"testing the flow"}' http://localhost:1880/sendMail
$ docker stop sendmailservice
</pre>

Deploy and run this container image in the Cloud Run environment.
--

Cloud Run console: https://console.cloud.google.com/run
<br />Artifact Registry console: https://console.cloud.google.com/artifacts

Using the project name drop down at the top of either of the above pages, select or create the project you want to work within ("email-alert-service" in my case)

Create a docker registry within that project and push the Node-RED container into that registry.  See this [HOWTO](https://cloud.google.com/artifact-registry/docs/docker/quickstart?hl=en_US).

Commands I used to build, test, and push my container from my development system:
<br /><i>(The "northamerica-northeast2-docker.pkg.dev/email-alert-service/docker-containers/" registry portion of these commands refers to my registry in the Google Cloud.  It won't work for you.  You'll need to create your own registry as mentioned above and use its name as part of these commands.)</i>
<pre>
$ cd ~/src/Particle/powerMonitor/nodered.docker
$ docker build -t northamerica-northeast2-docker.pkg.dev/email-alert-service/docker-containers/sendmailservice:latest .
$ docker run --rm -p 1880:1880 --name sendmailservice northamerica-northeast2-docker.pkg.dev/email-alert-service/docker-containers/sendmailservice:latest
$ curl --header "Content-Type: application/json" -d '{"address":"<i>phonenumber@sms.gateway.hostname.here</i>","subject":"","message":"testing the flow"}' http://localhost:1880/sendMail
$ curl --header "Content-Type: application/json" -d '{"address":"<i>email@address.here</i>","subject":"Test","message":"testing the flow"}' http://localhost:1880/sendMail
$ docker stop sendmailservice
$ docker push northamerica-northeast2-docker.pkg.dev/email-alert-service/docker-containers/sendmailservice:latest
</pre>

Open the container entry in the Artifact Registry console and select "Deploy to Cloud Run" from the the three-dot drop-down menu next to the digest entry.

* Make sure that region matches the registry's region.
* Make sure to select "Allow unauthorized invocations" on the second page under "Authentication" to make this a public service

Service URL will be displayed after creation process is complete.

Test the service:
<pre>
$ curl --header "Content-Type: application/json" -d '{"address":"<i>phonenumber@sms.gateway.hostname.here</i>","subject":"","message":"testing the flow"}' https://<service-url>/sendMail
$ curl --header "Content-Type: application/json" -d '{"address":"<i>email@address.here</i>","subject":"Test message","message":"testing the flow"}' https://<service-url>/sendMail
</pre>    
Node-RED's console messages are available in the service "Log" tab in the Cloud Run console.

To update an existing service:
--

1) Push the new version of the container to the registry using the exact same steps outlined above,
2) Open the service definition in the Google Cloud Run console and, under the "Revisions" tab, select "EDIT & DEPLOY NEW REVISION."   Select the new revision from the registry and deploy it in place of the old.
