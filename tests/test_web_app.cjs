// Run with Playwright on NODE_PATH and BROWSER_PATH pointing to Chrome.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const {chromium} = require('playwright');

(async () => {
    const browser = await chromium.launch({executablePath: process.env.BROWSER_PATH});
    try {
        const page = await browser.newPage({viewport: {width: 360, height: 780}});
        const devices = [
            {type: 'otc', token: 'OT' + '1'.repeat(30), server: 'relay.example', port: 8443},
            {type: 'ip', ip: 'bench.example:8081', devkey: 'old'}
        ];
        const commands = [];
        const key = ' A&+#/é ';
        let version = 127;
        let result = 1;
        await page.addInitScript(entries => {
            if (!localStorage.og_devices_v1) localStorage.og_devices_v1 = JSON.stringify({currdev: -1, devlist: entries});
            window.open = url => { window.lastOpened = url; };
        }, devices);
        const html = fs.readFileSync('web-app/og_devices.html', 'utf8');
        await page.route('**/*', async route => {
            const url = new URL(route.request().url());
            if (url.pathname === '/og_devices.html') return route.fulfill({contentType:'text/html', body:html});
            if (url.pathname.endsWith('/jc')) return route.fulfill({json:{fwv:version, name:'Bench', door:0, vehicle:0, dist:100, secv:2, light:0, lock:0}});
            if (url.pathname.endsWith('/cc')) {
                commands.push({url,method:route.request().method()});
                return route.fulfill({json:{result}});
            }
            return route.abort();
        });
        await page.goto('http://app.example/og_devices.html');
        await page.locator('#door0').filter({hasText:'closed'}).waitFor();
        const checkMissingKey = async selector => {
            const warning = page.waitForEvent('dialog').then(async dialog => {
                const message = dialog.message();
                await dialog.accept();
                return message;
            });
            const [message] = await Promise.all([warning, page.click(selector)]);
            assert.match(message, /No device key is saved.*Tap Edit to enter its key/);
            assert.equal(await page.locator('#popup_dev_add.visible, #popup_confirm.visible').count(), 0);
            assert.equal(commands.length, 0);
        };
        await page.click('#name0');
        await checkMissingKey('#btn_dev_click');
        await page.click('#btn_dev_got');
        await page.locator('#currdev-btn').filter({hasText:'Open'}).waitFor();
        for (const [button, suffix] of [['options','vo'], ['logs','vl']]) {
            await page.click('#btn_dev_' + button);
            assert.equal(await page.evaluate(() => window.lastOpened), 'https://relay.example:8443/forward/v1/' + devices[0].token + '/' + suffix);
        }
        await checkMissingKey('#currdev-btn');
        await checkMissingKey('label.switch:first-of-type:has(#sw_light)');
        assert.equal(await page.locator('#sw_light').isChecked(), false);
        await page.click('#btn_dev_list');
        await page.click('#btn_dev_edit');
        await page.fill('#da_devkey', key);
        await page.click('#btn_dev_add_submit');
        assert.equal(commands.length, 0);
        await page.click('#btn_dev_got');
        await page.click('#currdev-btn');
        await Promise.all([page.waitForResponse(r => r.url().includes('/cc?')), page.click('#btn_confirm_ok')]);
        assert.equal(commands.length, 1);
        assert.equal(commands[0].url.searchParams.get('dkey'), key);
        assert.equal(commands[0].url.searchParams.get('click'), '1');
        assert.equal(commands[0].method, 'GET');
        result = 2;
        await page.locator('label.switch').first().click();
        await page.locator('#popup_dev_add.visible').waitFor();
        assert.equal(commands.at(-1).url.searchParams.get('light'), 'toggle');
        await page.click('#btn_dev_add_cancel');
        result = 1;
        version = 126;
        await page.reload();
        await page.click('#currdev-btn');
        await Promise.all([page.waitForResponse(r => r.url().includes('/cc?')), page.click('#btn_confirm_ok')]);
        assert.equal(commands.at(-1).url.searchParams.get('dkey'), key);
        await page.evaluate(() => {
            const saved = JSON.parse(localStorage.og_devices_v1);
            delete saved.devlist[0].devkey;
            localStorage.og_devices_v1 = JSON.stringify(saved);
        });
        await page.reload();
        await page.click('#currdev-btn');
        await Promise.all([page.waitForResponse(r => r.url().includes('/cc?')), page.click('#btn_confirm_ok')]);
        assert.equal(commands.at(-1).url.searchParams.has('dkey'), false);
        await page.click('#btn_dev_list');
        await page.click('#name1');
        await page.click('#btn_dev_edit');
        await page.fill('#da_address', 'changed.example:8082');
        await page.fill('#da_devkey', key);
        await page.click('#btn_dev_add_submit');
        await page.click('#btn_dev_click');
        await Promise.all([page.waitForResponse(r => r.url().includes('/cc?')), page.click('#btn_confirm_ok')]);
        assert.equal(commands.at(-1).url.host, 'changed.example:8082');
        assert.equal(commands.at(-1).url.searchParams.get('dkey'), key);
        assert.equal(await page.locator('#ndevs').textContent(), '2');
        await page.click('#btn_dev_up');
        assert.equal(await page.locator('#lb_sel_msg').textContent(), 'ip (changed.example:8082)');
        await page.click('#btn_dev_add');
        assert.equal(await page.locator('#da_custom').isHidden(), true);
        await page.fill('#da_address', 'otgarage.local');
        assert.equal(await page.locator('#da_custom').isHidden(), true);
        await page.fill('#da_address', 'OT' + '2'.repeat(30));
        assert.equal(await page.locator('#da_custom').isVisible(), true);
        assert.equal(await page.locator('#da_server').isHidden(), true);
        assert.equal(await page.getByLabel('Device Address:', {exact:true}).getAttribute('aria-describedby'), 'da_hint');
        assert.equal(await page.locator('#da_hint').textContent(), '(May be an IP, URL, or OTC Token)');
        const summary = page.locator('#da_custom summary');
        const marker = () => summary.locator('.expand-icon').evaluate(el => getComputedStyle(el, '::before').content);
        assert.equal(await marker(), '"+"');
        await summary.press('Enter');
        await page.locator('#da_server').waitFor();
        assert.equal(await marker(), '"−"');
        await summary.press('Enter');
        await page.fill('#da_devkey', key);
        await page.click('#btn_dev_add_submit');
        await page.locator('#ndevs').filter({hasText:'3'}).waitFor();
        const saved = await page.evaluate(() => JSON.parse(localStorage.og_devices_v1).devlist);
        assert.equal(saved[2].server, 'cloud.openthings.io');
        assert.equal(saved[2].port, 443);
        assert.equal(saved[2].devkey, key);
        await page.click('#name2');
        await page.click('#btn_dev_edit');
        const forwardedToken = 'OT' + 'x'.repeat(30);
        await page.fill('#da_address', 'https://other.example:8443/forward/v1/' + forwardedToken + '/vo');
        await page.locator('#da_address').press('Tab');
        assert.equal(await page.inputValue('#da_address'), forwardedToken);
        assert.equal(await page.inputValue('#da_server'), 'other.example');
        assert.equal(await page.inputValue('#da_port'), '8443');
        await page.click('#btn_dev_add_submit');
        await page.click('#btn_dev_edit');
        await page.locator('#da_server').waitFor();
        await page.fill('#da_address', 'https://otgarage.local:8443/device/vo');
        await page.click('#btn_dev_add_submit');
        await page.click('#btn_dev_got');
        await page.locator('#currdev-btn').filter({hasText:'Open'}).waitFor();
        await page.click('#btn_dev_logs');
        assert.equal(await page.evaluate(() => window.lastOpened), 'https://otgarage.local:8443/device/vl');
        const converted = await page.evaluate(() => JSON.parse(localStorage.og_devices_v1).devlist[2]);
        assert.equal(converted.type, 'ip');
        assert.equal(converted.token, undefined);
        assert.equal(converted.devkey, key);
        await page.click('#btn_dev_list');
        let downloadFails = false;
        const cloudActions = [];
        await page.route('**/admin-ajax.php', route => {
            const action = new URLSearchParams(route.request().postData()).get('action');
            cloudActions.push(action);
            if (action === 'ajaxLogin') return route.fulfill({json:{token:'test'}});
            assert.equal(action, 'getSites');
            return downloadFails ? route.fulfill({status:503,body:''}) : route.fulfill({json:{sites:{v:1}}});
        });
        await page.click('#btn_cloud_sync');
        await page.fill('#cloud_user', 'tester');
        await page.fill('#cloud_pass', 'test');
        await page.click('#btn_cloud_login');
        const warning = page.locator('#popup_sync_choice.visible .modal-body p');
        await warning.waitFor();
        assert.match(await warning.textContent(), /account login password may have changed.*older format/);
        let confirmation;
        page.once('dialog', async dialog => { confirmation = dialog.message(); await dialog.dismiss(); });
        await page.click('#btn_sync_push');
        assert.match(confirmation, /Replace the unreadable cloud backup/);
        await page.click('#btn_sync_cancel');
        downloadFails = true;
        await page.click('#btn_cloud_sync');
        await page.click('#btn_cloud_login');
        await page.locator('#lb_cloud_msg').filter({hasText:'Could not download'}).waitFor();
        assert.deepEqual(cloudActions, ['ajaxLogin','getSites','ajaxLogin','getSites']);
        await page.evaluate(() => {
            const state = JSON.parse(localStorage.og_devices_v1);
            state.devlist.push({type:'blynk', name:'Old garage', token:'retired-token', image:'old.png'});
            localStorage.og_devices_v1 = JSON.stringify(state);
        });
        await page.reload();
        await page.click('#name3');
        await page.click('#btn_dev_edit');
        assert.equal(await page.inputValue('#da_address'), '');
        await page.fill('#da_address', '192.168.0.15:8080');
        await page.click('#btn_dev_add_submit');
        const migrated = await page.evaluate(() => JSON.parse(localStorage.og_devices_v1).devlist[3]);
        assert.equal(migrated.ip, '192.168.0.15:8080');
        assert.equal(migrated.image, 'old.png');
        assert.equal(migrated.token, undefined);
        console.log('PASS unified Add/Edit, URL and token routing, retired entries, device keys, and cloud sync');
    } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
