(function () {
    var endpoint = 'http://' + location.hostname + ':18765';
    var busy = false;
    function render() {
        var card = document.querySelector('.device_info_container');
        if (!card) return;
        var imsi = card.querySelector('[data-bind="text: imsi"]');
        var row = document.getElementById('zxct-ims-row');
        if (!imsi) return;
        if (!row) {
            row = document.createElement('div');
            row.id = 'zxct-ims-row';
            row.className = 'row';
            row.innerHTML = '<label class="col-xs-7">用户态 IMS</label><span class="col-xs-5"><span id="zxct-ims-state">查询中</span> <button id="zxct-ims-retry" type="button" style="font-size:14px;color:#333;line-height:normal">重试注册</button></span>';
            imsi.parentNode.parentNode.insertBefore(row, imsi.parentNode.nextSibling);
            card.style.height = (card.offsetHeight + 32) + 'px';
            var content = row.parentNode;
            content.style.height = (content.offsetHeight + 32) + 'px';
            document.getElementById('zxct-ims-retry').onclick = function () {
                var shade = document.createElement('div');
                shade.style.cssText = 'position:fixed;inset:0;background:rgba(0,0,0,.45);z-index:9999;display:flex;align-items:center;justify-content:center';
                shade.innerHTML = '<form style="background:white;color:#333;padding:20px;border-radius:4px;font-size:14px"><label>原厂后台密码 <input type="password" required autocomplete="current-password"></label><div style="margin-top:12px;text-align:right"><button type="button">取消</button> <button type="submit">重试注册</button></div></form>';
                document.body.appendChild(shade);
                var form = shade.querySelector('form'), input = shade.querySelector('input');
                shade.querySelector('button').onclick = function () { shade.remove(); };
                form.onsubmit = function (event) {
                    event.preventDefault();
                    var password = input.value;
                    input.value = ''; shade.remove();
                    busy = true; refresh();
                    var xhr = new XMLHttpRequest();
                    xhr.open('POST', endpoint + '/retry');
                    xhr.setRequestHeader('Content-Type', 'text/plain');
                    xhr.onload = function () {
                        busy = false;
                        if (xhr.status !== 200) window.alert(xhr.status === 403 ? '密码错误或当前不是支持的中国电信、联通卡。' : '无法重试注册，请检查设备状态。');
                        refresh();
                    };
                    xhr.onerror = function () { busy = false; window.alert('无法连接 IMS 状态服务。'); refresh(); };
                    xhr.send(encodeURIComponent(password));
                    password = '';
                };
                input.focus();
            };
        }
        var button = document.getElementById('zxct-ims-retry');
        button.disabled = busy || !/^460(01|03|05|06|09|11|12)\d{10}$/.test(imsi.textContent.trim());
        button.style.opacity = button.disabled ? '0.55' : '1';
    }
    function refresh() {
        render();
        var state = document.getElementById('zxct-ims-state');
        if (!state) return;
        var xhr = new XMLHttpRequest();
        xhr.open('GET', endpoint + '/status');
        xhr.onload = function () {
            var names = {registered: '已注册', starting: '注册中', waiting: '等待重试', stopped: '未运行'};
            state.textContent = xhr.status === 200 ? (names[xhr.responseText] || '未知') : '不可用';
        };
        xhr.onerror = function () { state.textContent = '不可用'; };
        xhr.send();
    }
    window.setInterval(refresh, 5000);
    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', refresh);
    else refresh();
}());
