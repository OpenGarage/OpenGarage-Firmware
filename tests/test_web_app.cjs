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
        await page.click('#name0');
        await page.click('#btn_dev_got');
        await page.locator('#currdev-btn').filter({hasText:'Open'}).waitFor();
        for (const [button, suffix] of [['options','vo'], ['logs','vl']]) {
            await page.click('#btn_dev_' + button);
            assert.equal(await page.evaluate(() => window.lastOpened), 'https://relay.example:8443/forward/v1/' + devices[0].token + '/' + suffix);
        }
        await page.click('#currdev-btn');
        await page.fill('#da_devkey', key);
        await page.click('#btn_dev_add_submit');
        assert.equal(commands.length, 0);
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
        await page.fill('#da_ip', 'changed.example:8082');
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
        assert.equal(await page.locator('#dev_addmenu a').count(), 2);
        await page.click('#btn_dev_add_by_otc');
        assert.deepEqual(await page.locator('#popup_dev_add .form-field:visible input').evaluateAll(
            inputs => inputs.map(input => input.id)), ['da_token', 'da_devkey', 'da_server', 'da_port']);
        for (const label of ['OTC Token:', 'OTC Server:']) {
            const input = page.getByLabel(label, {exact: true});
            assert.equal(await input.evaluate(el => {
                const range = document.createRange();
                range.selectNodeContents(el.labels[0]);
                return range.getClientRects().length;
            }), 1, label + ' should fit on one line');
        }
        await page.fill('#da_token', 'OT' + '2'.repeat(30));
        await page.fill('#da_devkey', key);
        await page.click('#btn_dev_add_submit');
        await page.locator('#ndevs').filter({hasText:'3'}).waitFor();
        const saved = await page.evaluate(() => JSON.parse(localStorage.og_devices_v1).devlist);
        assert.equal(saved[2].server, 'cloud.openthings.io');
        assert.equal(saved[2].port, 443);
        assert.equal(saved[2].devkey, key);
        console.log('PASS app editing, device links, keyed LAN and OTC controls, and legacy token-only commands');
    } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
