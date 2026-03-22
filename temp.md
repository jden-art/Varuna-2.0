```
The SIM800L module on the device has one responsibility: transmitting sensor data and system status to the server via HTTP POST. The device does not contact any authorities directly, does not manage contact lists, and does not wait for acknowledgments from anyone.

All alert routing — deciding who gets notified, through which channel, and at what escalation level — is handled entirely by the server. The server receives the device's status payload, evaluates the response level, and dispatches notifications to the appropriate authorities through SMS, WhatsApp, IVR calls, or email depending on the severity. Contact lists are managed through the server's web interface and can be updated at any time without any changes to the device. The escalation chain, cooldowns, and re-alert logic all live on the server side.

The device's role ends the moment the HTTP POST is confirmed. Everything after that is the server's responsibility.




What the device sends

Every transmit interval, the SIM800L makes one HTTP POST to the server. The payload already contains everything the server needs — water height, response level, zone, rate of change, GPS coordinates, battery, timestamp. The device does not decide anything about alerting. It just reports its state.

What the server receives and tracks

The server keeps a record of the last known response level for each device. Every time a new payload arrives, the server compares the incoming response level to the previously stored one. This comparison is the only trigger for any notification. If the level hasn't changed, no SMS is sent — no matter how many payloads come in. The server is not reacting to individual readings, it is reacting to level transitions.

When an SMS is actually sent

An SMS goes out only when the response level steps up. Specifically:

When the level moves from NORMAL to WATCH, only your internal operations team is messaged. No authorities yet.

When the level moves to WARNING, the server sends SMS to the Sarpanch or Ward officer and the Tahsildar's circle office. These are the closest ground-level officials who can physically verify and begin preparations.

When the level moves to FLOOD, the server sends SMS to the District Control Room, the District Collector's duty number, and the local police station duty officer. At this point NDRF/SDRF mobilization becomes possible.

When the level moves to CRITICAL, all the FLOOD-level contacts are re-messaged immediately with no cooldown, and the State Emergency Operations Centre is added. This is also when an IVR voice call is triggered for the most critical numbers — a recorded message that goes to voicemail even if no one picks up.

The cooldown logic

The cooldown now lives entirely on the server. Its purpose is different from the old device-side cooldown. It is not there to prevent the device from spamming — the device only sends one POST per interval anyway. The cooldown is there to handle sustained levels.

If the water rises to FLOOD and stays there, the level-change trigger fires once. After that, no new SMS goes out unless the level changes again. However, if the water is still at FLOOD after 30 minutes with no change, the server sends a reminder SMS to the FLOOD contacts — not as a new escalation, just a status update saying the situation is ongoing. The same logic applies at CRITICAL with a 15-minute reminder interval. At WARNING the reminder interval is 60 minutes. At WATCH there is no reminder — your ops team can see the dashboard.

These reminders are clearly worded differently from escalation alerts so recipients know the situation hasn't worsened, it's just persisting.

De-escalation

When the level steps down — say from FLOOD back to WARNING — the server sends a de-escalation message to everyone who received the FLOOD alert, telling them the level has dropped and monitoring is continuing. When the level returns fully to NORMAL, an all-clear message goes out to everyone who was contacted during the event. This gives officials a clean closure signal so they are not left wondering whether the situation resolved.

The message format

Since you're dealing with Indian government officials who receive messages from unknown numbers, the message format needs to look credible and official. Every message should contain the station name, the current water level in centimeters, the zone classification, the rate of rise, GPS coordinates, and a timestamp in IST. Something like:

VARUNA FLOOD ALERT | Station: Godavari-KG-01 | Level: 275cm (DANGER) | Rising at 12cm/15min | GPS: 16.5N 81.8E | 14:32 IST 21-Mar-2026

Short, structured, no abbreviations that an official wouldn't understand. The same format every time so that once a recipient has seen one message, they immediately know how to read the next one.

Contact management

All contacts are stored in the server database with their level assignment — which level triggers their notification. They are added and edited through the web interface. No SMS commands, no device-side storage, no need to touch the hardware at all. If a Tahsildar is transferred and replaced, you update one field in the database and the new person starts receiving alerts from the next event onward.

The one place SMS still touches the device

The device can still receive a single inbound SMS command for emergency recalibration or a manual status request, since sometimes field engineers need to interact with the device directly when there is no internet on the laptop they have with them. But this is a diagnostic channel only — it has no involvement in the alert pipeline whatsoever.

<svg width="100%" viewBox="0 0 680 700" xmlns="http://www.w3.org/2000/svg">
<defs>
  <marker id="arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
    <path d="M2 1L8 5L2 9" fill="none" stroke="context-stroke" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
  </marker>
<mask id="imagine-text-gaps-81ubpi" maskUnits="userSpaceOnUse"><rect x="0" y="0" width="680" height="700" fill="white"/><rect x="241.7303009033203" y="12.457036018371582" width="196.30914306640625" height="23.4287052154541" fill="black" rx="2"/><rect x="233.4427490234375" y="33.04753112792969" width="213.114501953125" height="19.5429630279541" fill="black" rx="2"/><rect x="45.9349479675293" y="64.45703887939453" width="56.91021728515625" height="23.428709030151367" fill="black" rx="2"/><rect x="5.114418983459473" y="79.04753112792969" width="139.84963989257812" height="19.5429630279541" fill="black" rx="2"/><rect x="314.6602478027344" y="64.45703887939453" width="151.32748413085938" height="23.428709030151367" fill="black" rx="2"/><rect x="248.77467346191406" y="79.04753112792969" width="281.8709716796875" height="19.5429630279541" fill="black" rx="2"/><rect x="36.49380874633789" y="122.45703125" width="76.30854797363281" height="23.4287052154541" fill="black" rx="2"/><rect x="22.85323143005371" y="137.0475311279297" width="102.9982681274414" height="19.5429630279541" fill="black" rx="2"/><rect x="179.24942016601562" y="114.45703125" width="168.2058563232422" height="23.4287052154541" fill="black" rx="2"/><rect x="197.52455139160156" y="131.0475311279297" width="133.94638061523438" height="19.5429630279541" fill="black" rx="2"/><rect x="190.39056396484375" y="143.0475311279297" width="148.21266174316406" height="19.5429630279541" fill="black" rx="2"/><rect x="430.6220703125" y="114.45703125" width="171.09463500976562" height="23.4287052154541" fill="black" rx="2"/><rect x="456.9823913574219" y="131.0475311279297" width="118.70305633544922" height="19.5429630279541" fill="black" rx="2"/><rect x="442.390625" y="143.0475311279297" width="148.21266174316406" height="19.5429630279541" fill="black" rx="2"/><rect x="48.67722702026367" y="195.45703125" width="52.95791244506836" height="23.4287052154541" fill="black" rx="2"/><rect x="23.935976028442383" y="210.0475311279297" width="103.23526000976562" height="19.5429630279541" fill="black" rx="2"/><rect x="183.15451049804688" y="188.45703125" width="84.41699981689453" height="23.4287052154541" fill="black" rx="2"/><rect x="166.0937042236328" y="205.0475311279297" width="117.82756042480469" height="19.5429630279541" fill="black" rx="2"/><rect x="152.67575073242188" y="217.0475311279297" width="144.74656677246094" height="19.5429630279541" fill="black" rx="2"/><rect x="336.02642822265625" y="188.45703125" width="115.24504852294922" height="23.4287052154541" fill="black" rx="2"/><rect x="327.29364013671875" y="205.0475311279297" width="132.41786193847656" height="19.5429630279541" fill="black" rx="2"/><rect x="329.287109375" y="217.0475311279297" width="127.69081115722656" height="19.5429630279541" fill="black" rx="2"/><rect x="516.6604614257812" y="188.45703125" width="83.0098876953125" height="23.4287052154541" fill="black" rx="2"/><rect x="503.6675109863281" y="205.0475311279297" width="109.6598129272461" height="19.5429630279541" fill="black" rx="2"/><rect x="542.2415771484375" y="217.0475311279297" width="31.618019104003906" height="19.5429630279541" fill="black" rx="2"/><rect x="41.89741897583008" y="267.45703125" width="67.12244415283203" height="23.4287052154541" fill="black" rx="2"/><rect x="25.433605194091797" y="282.0475158691406" width="100.13720703125" height="19.5429630279541" fill="black" rx="2"/><rect x="157.63584899902344" y="260.45703125" width="131.8745346069336" height="23.4287052154541" fill="black" rx="2"/><rect x="159.8317108154297" y="277.04754638671875" width="128.99864959716797" height="19.5429630279541" fill="black" rx="2"/><rect x="185.4229736328125" y="289.0475158691406" width="77.15404510498047" height="19.5429630279541" fill="black" rx="2"/><rect x="356.8331298828125" y="260.45703125" width="70.69903564453125" height="23.4287052154541" fill="black" rx="2"/><rect x="327.4674377441406" y="277.04754638671875" width="129.25638580322266" height="19.5429630279541" fill="black" rx="2"/><rect x="349.5777282714844" y="291.0475158691406" width="84.84457397460938" height="19.5429630279541" fill="black" rx="2"/><rect x="501.562744140625" y="260.45703125" width="112.99354553222656" height="23.4287052154541" fill="black" rx="2"/><rect x="506.9360046386719" y="277.04754638671875" width="102.1280288696289" height="19.5429630279541" fill="black" rx="2"/><rect x="500.02459716796875" y="291.0475158691406" width="116.14379119873047" height="19.5429630279541" fill="black" rx="2"/><rect x="161.5464630126953" y="343.04754638671875" width="357.1065368652344" height="19.5429630279541" fill="black" rx="2"/><rect x="80.91419219970703" y="378.4570617675781" width="36.31732177734375" height="23.4287052154541" fill="black" rx="2"/><rect x="49.01873779296875" y="395.04754638671875" width="100.70144653320312" height="19.5429630279541" fill="black" rx="2"/><rect x="40.09367752075195" y="409.04754638671875" width="116.70704650878906" height="19.5429630279541" fill="black" rx="2"/><rect x="214.5545196533203" y="378.4570617675781" width="76.22227478027344" height="23.4287052154541" fill="black" rx="2"/><rect x="200.83302307128906" y="395.04754638671875" width="104.33399200439453" height="19.5429630279541" fill="black" rx="2"/><rect x="200.55979919433594" y="409.04754638671875" width="103.67552185058594" height="19.5429630279541" fill="black" rx="2"/><rect x="379.594482421875" y="378.4570617675781" width="55.0265007019043" height="23.4287052154541" fill="black" rx="2"/><rect x="353.7603759765625" y="395.04754638671875" width="107.14833068847656" height="19.5429630279541" fill="black" rx="2"/><rect x="366.05511474609375" y="409.04754638671875" width="82.55722045898438" height="19.5429630279541" fill="black" rx="2"/><rect x="516.1008911132812" y="378.4570617675781" width="109.90618896484375" height="23.4287052154541" fill="black" rx="2"/><rect x="514.4615478515625" y="395.04754638671875" width="113.74856567382812" height="19.5429630279541" fill="black" rx="2"/><rect x="502.67279052734375" y="409.04754638671875" width="136.67942810058594" height="19.5429630279541" fill="black" rx="2"/><rect x="293.07470703125" y="457.0475158691406" width="93.9410629272461" height="19.5429630279541" fill="black" rx="2"/><rect x="50.92656707763672" y="486.45703125" width="248.4693145751953" height="23.4287052154541" fill="black" rx="2"/><rect x="45.76581573486328" y="503.04754638671875" width="258.4683837890625" height="19.5429630279541" fill="black" rx="2"/><rect x="390.4969482421875" y="486.45703125" width="208.32199096679688" height="23.4287052154541" fill="black" rx="2"/><rect x="372.4950256347656" y="503.04754638671875" width="245.00994873046875" height="19.5429630279541" fill="black" rx="2"/><rect x="65.63976287841797" y="538.45703125" width="219.1489715576172" height="23.4287052154541" fill="black" rx="2"/><rect x="64.85047149658203" y="555.0475463867188" width="220.299072265625" height="19.5429630279541" fill="black" rx="2"/><rect x="382.6445007324219" y="538.45703125" width="225.04103088378906" height="23.4287052154541" fill="black" rx="2"/><rect x="375.6420593261719" y="555.0475463867188" width="238.79400634765625" height="19.5429630279541" fill="black" rx="2"/><rect x="75.6756362915039" y="591.0475463867188" width="528.6488037109375" height="19.5429630279541" fill="black" rx="2"/><rect x="133.74925231933594" y="607.0475463867188" width="412.5015869140625" height="19.5429630279541" fill="black" rx="2"/></mask></defs>

<!-- Title -->
<text x="340" y="30" text-anchor="middle" style="fill:rgb(250, 249, 245);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">Who gets alerted — and how</text>
<text x="340" y="48" text-anchor="middle" style="fill:rgb(194, 192, 182);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">Server dispatches. SIM only posts data.</text>

<!-- ===== WATCH ===== -->
<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="30" y="64" width="90" height="36" rx="6" stroke-width="0.5" style="fill:rgb(8, 80, 65);stroke:rgb(93, 202, 165);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="75" y="82" text-anchor="middle" style="fill:rgb(159, 225, 203);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">WATCH</text>
  <text x="75" y="94" text-anchor="middle" style="fill:rgb(93, 202, 165);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">rising, not yet dangerous</text>
</g>
<line x1="120" y1="82" x2="148" y2="82" marker-end="url(#arrow)" stroke="#1D9E75" mask="url(#imagine-text-gaps-81ubpi)" style="fill:none;stroke:rgb(156, 154, 146);color:rgb(255, 255, 255);stroke-width:1.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="150" y="64" width="480" height="36" rx="6" stroke-width="0.5" style="fill:rgb(8, 80, 65);stroke:rgb(93, 202, 165);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="390" y="82" text-anchor="middle" style="fill:rgb(159, 225, 203);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">Internal ops team only</text>
  <text x="390" y="94" text-anchor="middle" style="fill:rgb(93, 202, 165);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">your engineers / field staff — via SMS or WhatsApp</text>
</g>

<!-- ===== WARNING ===== -->
<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="30" y="122" width="90" height="36" rx="6" stroke-width="0.5" style="fill:rgb(99, 56, 6);stroke:rgb(239, 159, 39);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="75" y="140" text-anchor="middle" style="fill:rgb(250, 199, 117);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">WARNING</text>
  <text x="75" y="152" text-anchor="middle" style="fill:rgb(239, 159, 39);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">threshold crossed</text>
</g>
<line x1="120" y1="140" x2="148" y2="140" marker-end="url(#arrow)" stroke="#BA7517" mask="url(#imagine-text-gaps-81ubpi)" style="fill:none;stroke:rgb(156, 154, 146);color:rgb(255, 255, 255);stroke-width:1.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>

<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="150" y="110" width="228" height="58" rx="6" stroke-width="0.5" style="fill:rgb(99, 56, 6);stroke:rgb(239, 159, 39);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="264" y="132" text-anchor="middle" style="fill:rgb(250, 199, 117);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">Village Panchayat / Ward</text>
  <text x="264" y="146" text-anchor="middle" style="fill:rgb(239, 159, 39);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">Sarpanch / Ward officer</text>
  <text x="264" y="158" text-anchor="middle" style="fill:rgb(239, 159, 39);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">SMS to registered number</text>
</g>
<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="402" y="110" width="228" height="58" rx="6" stroke-width="0.5" style="fill:rgb(99, 56, 6);stroke:rgb(239, 159, 39);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="516" y="132" text-anchor="middle" style="fill:rgb(250, 199, 117);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">Revenue / Tahsildar office</text>
  <text x="516" y="146" text-anchor="middle" style="fill:rgb(239, 159, 39);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">Circle officer on duty</text>
  <text x="516" y="158" text-anchor="middle" style="fill:rgb(239, 159, 39);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">SMS to registered number</text>
</g>

<!-- ===== FLOOD ===== -->
<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="30" y="196" width="90" height="36" rx="6" stroke-width="0.5" style="fill:rgb(113, 43, 19);stroke:rgb(240, 153, 123);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="75" y="213" text-anchor="middle" style="fill:rgb(245, 196, 179);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">FLOOD</text>
  <text x="75" y="225" text-anchor="middle" style="fill:rgb(240, 153, 123);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">active flood event</text>
</g>
<line x1="120" y1="214" x2="148" y2="214" marker-end="url(#arrow)" stroke="#993C1D" mask="url(#imagine-text-gaps-81ubpi)" style="fill:none;stroke:rgb(156, 154, 146);color:rgb(255, 255, 255);stroke-width:1.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>

<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="150" y="184" width="150" height="58" rx="6" stroke-width="0.5" style="fill:rgb(113, 43, 19);stroke:rgb(240, 153, 123);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="225" y="206" text-anchor="middle" style="fill:rgb(245, 196, 179);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">NDRF/SDRF</text>
  <text x="225" y="220" text-anchor="middle" style="fill:rgb(240, 153, 123);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">District control room</text>
  <text x="225" y="232" text-anchor="middle" style="fill:rgb(240, 153, 123);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">registered landline + SMS</text>
</g>
<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="316" y="184" width="154" height="58" rx="6" stroke-width="0.5" style="fill:rgb(113, 43, 19);stroke:rgb(240, 153, 123);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="393" y="206" text-anchor="middle" style="fill:rgb(245, 196, 179);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">District Collector</text>
  <text x="393" y="220" text-anchor="middle" style="fill:rgb(240, 153, 123);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">DM office duty number</text>
  <text x="393" y="232" text-anchor="middle" style="fill:rgb(240, 153, 123);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">SMS (Twilio/Fast2SMS)</text>
</g>
<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="486" y="184" width="144" height="58" rx="6" stroke-width="0.5" style="fill:rgb(113, 43, 19);stroke:rgb(240, 153, 123);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="558" y="206" text-anchor="middle" style="fill:rgb(245, 196, 179);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">Local police</text>
  <text x="558" y="220" text-anchor="middle" style="fill:rgb(240, 153, 123);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">Station duty officer</text>
  <text x="558" y="232" text-anchor="middle" style="fill:rgb(240, 153, 123);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">SMS</text>
</g>

<!-- ===== CRITICAL ===== -->
<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="30" y="268" width="90" height="36" rx="6" stroke-width="0.5" style="fill:rgb(121, 31, 31);stroke:rgb(240, 149, 149);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="75" y="285" text-anchor="middle" style="fill:rgb(247, 193, 193);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">CRITICAL</text>
  <text x="75" y="297" text-anchor="middle" style="fill:rgb(240, 149, 149);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">imminent danger</text>
</g>
<line x1="120" y1="286" x2="148" y2="286" marker-end="url(#arrow)" stroke="#A32D2D" mask="url(#imagine-text-gaps-81ubpi)" style="fill:none;stroke:rgb(156, 154, 146);color:rgb(255, 255, 255);stroke-width:1.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>

<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="150" y="256" width="148" height="58" rx="6" stroke-width="0.5" style="fill:rgb(121, 31, 31);stroke:rgb(240, 149, 149);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="224" y="278" text-anchor="middle" style="fill:rgb(247, 193, 193);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">All FLOOD contacts</text>
  <text x="224" y="292" text-anchor="middle" style="fill:rgb(240, 149, 149);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">re-alerted immediately</text>
  <text x="224" y="304" text-anchor="middle" style="fill:rgb(240, 149, 149);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">no cooldown</text>
</g>
<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="314" y="256" width="156" height="58" rx="6" stroke-width="0.5" style="fill:rgb(121, 31, 31);stroke:rgb(240, 149, 149);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="392" y="278" text-anchor="middle" style="fill:rgb(247, 193, 193);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">State EOC</text>
  <text x="392" y="292" text-anchor="middle" style="fill:rgb(240, 149, 149);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">Emergency Ops Centre</text>
  <text x="392" y="306" text-anchor="middle" style="fill:rgb(240, 149, 149);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">SMS + IVR call</text>
</g>
<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="486" y="256" width="144" height="58" rx="6" stroke-width="0.5" style="fill:rgb(121, 31, 31);stroke:rgb(240, 149, 149);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="558" y="278" text-anchor="middle" style="fill:rgb(247, 193, 193);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">Public broadcast</text>
  <text x="558" y="292" text-anchor="middle" style="fill:rgb(240, 149, 149);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">Common Alerting</text>
  <text x="558" y="306" text-anchor="middle" style="fill:rgb(240, 149, 149);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">Protocol (CAP) if live</text>
</g>

<!-- Divider -->
<line x1="30" y1="340" x2="650" y2="340" stroke="var(--color-border-tertiary)" stroke-width="0.5" stroke-dasharray="4 4" style="fill:rgb(0, 0, 0);stroke:rgba(222, 220, 209, 0.15);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-dasharray:4px, 4px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
<text x="340" y="358" text-anchor="middle" style="fill:rgb(194, 192, 182);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">How the message reaches them — from the server, not the device</text>

<!-- Channel boxes -->
<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="30" y="372" width="138" height="60" rx="8" stroke-width="0.5" style="fill:rgb(39, 80, 10);stroke:rgb(151, 196, 89);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="99" y="396" text-anchor="middle" style="fill:rgb(192, 221, 151);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">SMS</text>
  <text x="99" y="410" text-anchor="middle" style="fill:rgb(151, 196, 89);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">Fast2SMS / Twilio</text>
  <text x="99" y="424" text-anchor="middle" style="fill:rgb(151, 196, 89);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">works on any phone</text>
</g>

<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="184" y="372" width="138" height="60" rx="8" stroke-width="0.5" style="fill:rgb(39, 80, 10);stroke:rgb(151, 196, 89);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="253" y="396" text-anchor="middle" style="fill:rgb(192, 221, 151);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">WhatsApp</text>
  <text x="253" y="410" text-anchor="middle" style="fill:rgb(151, 196, 89);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">Meta Business API</text>
  <text x="253" y="424" text-anchor="middle" style="fill:rgb(151, 196, 89);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">for staff + officers</text>
</g>

<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="338" y="372" width="138" height="60" rx="8" stroke-width="0.5" style="fill:rgb(39, 80, 10);stroke:rgb(151, 196, 89);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="407" y="396" text-anchor="middle" style="fill:rgb(192, 221, 151);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">IVR call</text>
  <text x="407" y="410" text-anchor="middle" style="fill:rgb(151, 196, 89);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">Exotel / Knowlarity</text>
  <text x="407" y="424" text-anchor="middle" style="fill:rgb(151, 196, 89);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">CRITICAL only</text>
</g>

<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="492" y="372" width="158" height="60" rx="8" stroke-width="0.5" style="fill:rgb(39, 80, 10);stroke:rgb(151, 196, 89);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="571" y="396" text-anchor="middle" style="fill:rgb(192, 221, 151);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">Dashboard alert</text>
  <text x="571" y="410" text-anchor="middle" style="fill:rgb(151, 196, 89);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">browser push notify</text>
  <text x="571" y="424" text-anchor="middle" style="fill:rgb(151, 196, 89);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">always-on for your team</text>
</g>

<!-- Divider 2 -->
<line x1="30" y1="454" x2="650" y2="454" stroke="var(--color-border-tertiary)" stroke-width="0.5" stroke-dasharray="4 4" style="fill:rgb(0, 0, 0);stroke:rgba(222, 220, 209, 0.15);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-dasharray:4px, 4px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
<text x="340" y="472" text-anchor="middle" style="fill:rgb(194, 192, 182);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">Key design rules</text>

<!-- Rules -->
<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="30" y="486" width="290" height="38" rx="6" stroke-width="0.5" style="fill:rgb(68, 68, 65);stroke:rgb(180, 178, 169);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="175" y="504" text-anchor="middle" style="fill:rgb(211, 209, 199);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">One SMS per person per level-change</text>
  <text x="175" y="518" text-anchor="middle" style="fill:rgb(180, 178, 169);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">cooldown resets only when level changes again</text>
</g>
<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="340" y="486" width="310" height="38" rx="6" stroke-width="0.5" style="fill:rgb(68, 68, 65);stroke:rgb(180, 178, 169);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="495" y="504" text-anchor="middle" style="fill:rgb(211, 209, 199);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">All contacts stored in server DB</text>
  <text x="495" y="518" text-anchor="middle" style="fill:rgb(180, 178, 169);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">edit via web UI — no SMS command needed</text>
</g>
<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="30" y="538" width="290" height="38" rx="6" stroke-width="0.5" style="fill:rgb(68, 68, 65);stroke:rgb(180, 178, 169);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="175" y="556" text-anchor="middle" style="fill:rgb(211, 209, 199);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">No ACK expected from recipients</text>
  <text x="175" y="570" text-anchor="middle" style="fill:rgb(180, 178, 169);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">dashboard ACK for your team is enough</text>
</g>
<g style="fill:rgb(0, 0, 0);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto">
  <rect x="340" y="538" width="310" height="38" rx="6" stroke-width="0.5" style="fill:rgb(68, 68, 65);stroke:rgb(180, 178, 169);color:rgb(255, 255, 255);stroke-width:0.5px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:16px;font-weight:400;text-anchor:start;dominant-baseline:auto"/>
  <text x="495" y="556" text-anchor="middle" style="fill:rgb(211, 209, 199);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:14px;font-weight:500;text-anchor:middle;dominant-baseline:auto">IVR call = recorded voice message</text>
  <text x="495" y="570" text-anchor="middle" style="fill:rgb(180, 178, 169);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">no one needs to pick up — voicemail works</text>
</g>

<!-- Bottom note -->
<text x="340" y="606" text-anchor="middle" style="fill:rgb(194, 192, 182);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">Government contacts are registered once in the DB. The device never needs to know who they are.</text>
<text x="340" y="622" text-anchor="middle" style="fill:rgb(194, 192, 182);stroke:none;color:rgb(255, 255, 255);stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;opacity:1;font-family:&quot;Anthropic Sans&quot;, -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, sans-serif;font-size:12px;font-weight:400;text-anchor:middle;dominant-baseline:auto">If the contact list changes, update the server. No reflash, no SMS commands.</text>

</svg>

The SIM's only job is to **push data to the server**. The server handles who gets notified and how.

Now for the "who do we alert" question — this is actually a well-established pattern in Indian disaster management. Let me lay it out practically:**The real-world India escalation chain:**

**WATCH** — only your own team gets notified. No point disturbing officials over a sensor that's just risen a bit.

**WARNING** — the Sarpanch/Ward officer and the Tahsildar's circle office. These are the people closest to the ground who can physically verify and start moving people.

**FLOOD** — the District Control Room (this is a real 24/7 number every district has), the District Collector's duty number, and the local police station. These three together can mobilize NDRF/SDRF and issue evacuations.

**CRITICAL** — everyone above, plus the State Emergency Operations Centre, plus an IVR voice call for the most critical contacts. A recorded voice message saying *"Flood alert — water level 280cm at Station X, GPS coordinates Y — please respond"* works even if they ignore an SMS from an unknown number.

**On the IVR point specifically** — services like Exotel and Knowlarity are Indian, cheap, and built exactly for this. You configure a recorded message, give it a list of numbers, it calls each one. No one needs to pick up — it logs to voicemail. This is far more likely to get attention than an SMS from an unknown number at 3am.

**The SMS message itself** also matters. Government officials respond better to messages that look official — *"VARUNA FLOOD ALERT | Station: Godavari-KG-01 | Water level: 275cm (DANGER zone) | Rate: +12cm/15min | GPS: 16.5°N 81.8°E | Issued: 14:32 IST"* is taken seriously. *"VARUNA FLOOD ALERT Level:275cm Zone:DANGER"* looks like spam.

**What this means for your firmware** — literally nothing changes in the `.ino` file except removing the tier contact system and the ACK tracking. The `gprsPostData()` function you already have is perfect. The server does everything else.

```
























```

The current algorithm is too heavy for the buoy. The buoy is running a 3D decision matrix, sustained rise detection, step-down hysteresis, time gates, cold start logic, and ACK tracking — all of that belongs on the server. The buoy's CPU should not be doing flood intelligence. It should be doing one thing: **measuring accurately and transmitting urgently based on how alarming the raw numbers look.**

Here is the principle: **the buoy does not decide the flood level. It decides the transmission urgency.**

---

## What stays on the buoy

Only three things:

**1. Raw measurement** — water height, rate of change, battery, GPS, sensor health. No interpretation beyond the physics.

**2. Transmission urgency** — a simple 4-level escalation matrix that answers only one question: *how fast must I send this right now?* The buoy has no memory of previous states, no hysteresis, no time gates. It looks at the current reading and current rate, picks a transmission interval, and sends.

**3. Immediate trigger transmits** — certain conditions bypass the interval entirely and transmit right now regardless of when the last transmit was.

---

## The Escalation Matrix — Buoy Side OnlyGood. Now let me build the complete answer with the diagram.

The key insight is this: the current code treats the buoy as a **decision maker**. It decides what level of emergency exists, holds state, manages hysteresis, and dispatches alerts. That is wrong. The buoy is a **sensor node**. Its only intelligence should be: *given what I am reading right now, how urgently must I transmit?*

The server does everything else — pattern recognition, sustained rise, hysteresis, de-escalation, alert routing, cooldowns.

Here is the buoy-side escalation matrix:Now the full explanation of the logic and why each decision was made this way.

---

## The core principle

The buoy has exactly two variables it looks at to decide its transmit tier: **current water height** relative to the three thresholds, and **current rate of change** per 15 minutes. That is it. No memory of previous tiers, no step-down counters, no time gates. Every reading is evaluated fresh. The tier is stateless on the buoy.

The rate of change calculation stays on the buoy because it requires two consecutive readings and arithmetic — that is trivial computation the buoy must do anyway to know its own urgency. Everything beyond that — sustained rise detection, hysteresis, de-escalation logic, alert dispatch — lives on the server.

---

## Why the thresholds for each tier are OR conditions, not AND

In the current firmware the matrix requires both zone AND rate to be elevated before escalating. This is appropriate for a system that manages its own alerts and wants to avoid false alarms. But since the server now handles false alarm suppression, the buoy should be conservative in the opposite direction — it should transmit urgently whenever **either** condition is alarming, not only when both are. A fast rate in a normal zone is dangerous. A high level with a slow rate is dangerous. The buoy should not wait for both to be true simultaneously before increasing transmit frequency.

---

## The four transmit intervals and their reasoning

**30 minutes** for Tier 0 is a heartbeat. The server needs to know the device is alive and the river is stable. This is not flood monitoring interval — it is a watchdog interval.

**10 minutes** for Tier 1 gives the server enough data points to build a credible trend before anything dangerous happens. The alert threshold is the first sign of trouble. At 10-minute intervals over the course of a rising river, the server gets 3-4 readings before the warning threshold is reached — enough to establish whether the rise is genuine or a transient.

**2 minutes** for Tier 2 is where flood intelligence on the server side becomes meaningful. At 2-minute intervals the server can calculate its own rate of change, detect sustained rise across multiple readings, and make confident escalation decisions. If the rate is 5 cm/15 min, the buoy crosses 15 cm of rise in 45 minutes — at 2-minute intervals the server sees 22 readings in that window.

**30 seconds** for Tier 3 is the absolute maximum urgency the SIM800L can sustain without overloading the GPRS session. At danger threshold with an extreme rate, the situation can become catastrophic within minutes. 30-second transmits give the server near-real-time data and allow it to escalate every tier of authorities without waiting.

---

## The immediate triggers — why each one exists

**Tier jump** — when a single reading pushes the buoy from Tier 0 to Tier 2 without passing through Tier 1, the server would not know about it until the next scheduled transmit on the new interval. Transmitting immediately on any upward jump ensures the server sees the step-change in the same reading that caused it.

**Rate spike** — a jump of more than 15 cm/15min in a single step is physically unusual. It suggests either a dam opening upstream, a levee breach, or a flash flood front arriving. This is the most time-critical scenario and must reach the server immediately regardless of which tier the height puts the buoy in.

**Submersion** — when the BMP280 confirms the buoy is going underwater, the tilt-based measurement has already become unreliable. The buoy needs to transmit its last valid reading and its submersion state before the data quality degrades further. After this point the pressure-based depth calculation takes over, and those readings must also arrive at the server in near real time.

**Sensor fault** — if the MPU6050 fails and the HC-SR04 takes over, the server must know immediately. A reading gap followed by resumed data on a different sensor looks like a data anomaly to the server's algorithm. An explicit sensor fault transmission tells the server to update its model of the device's data quality.

**Battery critical** — when battery drops below 10% the buoy may shut down within hours. The server must receive a final valid state packet so it knows the last confirmed water level before the device goes silent. Without this the server cannot distinguish between "device went offline because the flood destroyed it" and "battery died during a calm period."

**Post-sleep wake** — after any light sleep period, the first reading must transmit immediately rather than waiting for the next interval. The sleep may have lasted 25 minutes. Conditions could have changed significantly. The server should not be extrapolating across a 25-minute gap when it can have a fresh reading the moment the buoy wakes.

---

## What the buoy's payload must always contain

Since the server is doing all the intelligence now, the payload from every transmit must be complete. The server cannot ask follow-up questions. Every POST must include: water height, rate of change, current tier (so the server knows why this transmit happened), battery percentage, GPS coordinates, active sensor flag (MPU6050 or HC-SR04), submersion state, pressure deviation, timestamp, and a sensor health byte that encodes the status of all sensors in a single field. The server stores all of this and runs its own flood detection algorithm across the incoming stream.

---

## What gets removed from the firmware entirely

The `sustainedBuffer` and all associated logic. The `lookupDecisionMatrix()` function. The `evaluateStepDown()` function. The `canStepDown()` function. All the step-down consecutive reading counters and time gates. The `RESP_WATCH`, `RESP_WARNING`, `RESP_FLOOD`, `RESP_CRITICAL` response levels as internal state — the buoy no longer tracks its own response level. The `currentZone` classification. The `floodAlertLevel` variable. The cold-start level detection logic. All the `dispatchAlerts()`, `dispatchDeescalation()`, and `dispatchAllClear()` functions. The ACK system entirely.

What replaces all of that is one function: `classifyTransmitTier(waterHeight, ratePer15Min)` that returns 0, 1, 2, or 3, and one function: `shouldTransmitImmediately(previousTier, currentTier, ratePrevious, rateCurrent, submersionState, sensorChanged, batteryPercent)` that returns true or false. These two functions are the entire flood intelligence remaining on the buoy. Everything else moves to the server.
```
## 37. Serial CSV Output Format — 38 Fields

```
Field  Content                    Example
─────  ────────────────────────   ─────────
 1     theta (degrees)            12.45
 2     waterHeight (cm)           45.23
 3     correctedTiltX (deg)       10.32
 4     correctedTiltY (deg)       6.89
 5     olpLength (cm)             100.00
 6     horizontalDist (cm)        21.56
 7     currentPressure (hPa)      1013.25
 8     currentTemperature (°C)    28.50
 9     baselinePressure (hPa)     1013.00
10     pressureDeviation (hPa)    0.25
11     submersionState (0-3)      0
12     estimatedDepth (cm)        0.00
13     bmpAvailable (0/1)         1
14     unixTime                   1706000000
15     dateTimeString             2024-01-23 10:30:00
16     rtcValid (0/1)             1
17     ratePer15Min (cm/15m)      1.234
18     floodAlertLevel (0-3)      0
19     sessionDuration (sec)      3600
20     peakHeight (cm)            48.50
21     minHeight (cm)             12.30
22     latitude                   12.971600
23     longitude                  77.594600
24     altitude (m)               920.5
25     gpsSatellites              8
26     gpsFixValid (0/1)          1
27     simSignalRSSI              18
28     simRegistered (0/1)        1
29     simAvailable (0/1)         1
30     currentZone (0-3)          0
31     currentResponseLevel (0-4) 0
32     sustainedRise (0/1)        0
33     batteryPercent             85.5
34     sampleInterval (sec)       1800
35     transmitInterval (sec)     3600
36     obLightEnabled (0/1)       1
37     debugEnabled (0/1)         0
38     algorithmEnabled (0/1)     0
