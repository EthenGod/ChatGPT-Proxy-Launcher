# SPDX-License-Identifier: GPL-3.0-only
# Keep this entry point ASCII so Windows PowerShell can execute it from an HTTP response.
[CmdletBinding()]
param([switch]$Headless, [string]$Request, [string]$Result,
    [ValidateSet('deploy', 'plan', 'info', 'companion-register', 'companion-configure', 'companion-remove', 'companion-list')]
    [string]$Operation = 'deploy', [string]$Manifest, [string]$Id, [switch]$Replace,
    [ValidateSet('Stable', 'Preview')][string]$Channel = 'Stable',
    [string]$Branch, [string]$Tag)
& {
    param($entryParameters)
    $ErrorActionPreference = 'Stop'
    $Operation = $Operation.ToLowerInvariant()
    # BEGIN GENERATED LOCALES
    $setupLocales = ConvertFrom-Json -InputObject @'
{"zh-CN":{"download.activity":"\u51c6\u5907 ChatGPT \u914d\u7f6e\u5668","download.browsererror":"\u65e0\u6cd5\u6253\u5f00\u6d4f\u89c8\u5668\uff0c\u8bf7\u624b\u52a8\u6253\u5f00\uff1a{url}","download.cancel":"\u53d6\u6d88","download.cancelled":"\u5df2\u53d6\u6d88\uff0c\u672a\u6253\u5f00\u914d\u7f6e\u5668\u3002","download.checksum":"\u6b63\u5728\u4e0b\u8f7d\u6821\u9a8c\u6587\u4ef6","download.cleaned":"\u4e34\u65f6\u6587\u4ef6\u5df2\u6e05\u7406\u3002","download.close":"\u9000\u51fa\u8bf7\u76f4\u63a5\u5173\u95ed\u6b64\u7a97\u53e3\u3002","download.compatibility":"\u4f7f\u7528\u517c\u5bb9\u754c\u9762","download.graphicalfailed":"\u914d\u7f6e\u5668\u5f02\u5e38\u9000\u51fa\uff0c\u9000\u51fa\u7801\u4e3a {code}\u3002","download.headless":"\u8fd9\u4e9b\u53c2\u6570\u9700\u8981\u540c\u65f6\u6307\u5b9a -Headless\u3002","download.incompletecleanup":"\u672a\u80fd\u6e05\u7406\u5168\u90e8\u4e34\u65f6\u4e0b\u8f7d\u6587\u4ef6\u3002","download.incompleteresult":"\u914d\u7f6e\u5668\u8fd4\u56de\u7684\u7ed3\u679c\u4e0d\u5b8c\u6574\u3002","download.inconsistentresult":"\u914d\u7f6e\u5668\u8fd4\u56de\u7684\u72b6\u6001\u4e0e\u9000\u51fa\u7801\u4e0d\u4e00\u81f4\u3002","download.invalidbranch":"\u5206\u652f\u540d\u79f0\u65e0\u6548\uff0c\u6216\u65e0\u6cd5\u6838\u5b9e\u8be5\u5206\u652f\u5bf9\u5e94\u7684\u63d0\u4ea4\u3002","download.invalidchecksum":"\u914d\u7f6e\u5668\u7684\u6821\u9a8c\u503c\u7f3a\u5931\u6216\u65e0\u6548\uff0c\u672a\u542f\u52a8\u914d\u7f6e\u5668\u3002","download.invalidresult":"\u914d\u7f6e\u5668\u8fd4\u56de\u7684\u7ed3\u679c\u65e0\u6548\u3002","download.keepfolder":"\u4e34\u65f6\u76ee\u5f55\u5df2\u4fdd\u7559\uff1a{path}","download.latest":"\u6b63\u5728\u67e5\u8be2\u53d1\u5e03\u7248\u672c","download.managedresult":"Result \u4e0d\u80fd\u4f4d\u4e8e\u672c\u5de5\u5177\u7ba1\u7406\u7684\u76ee\u5f55\u4e2d\u3002","download.manifestsize":"Manifest \u6587\u4ef6\u4e0d\u5b58\u5728\u6216\u8d85\u8fc7\u5927\u5c0f\u9650\u5236\u3002","download.mismatch":"SHA-256 \u6821\u9a8c\u4e0d\u4e00\u81f4\uff0c\u672a\u542f\u52a8\u914d\u7f6e\u5668\u3002","download.norelease":"\u6ca1\u6709\u7b26\u5408\u6240\u9009\u901a\u9053\u3001\u5206\u652f\u548c\u6807\u7b7e\u7684\u5df2\u53d1\u5e03\u7248\u672c\u3002","download.oldversion":"\u4e0b\u8f7d\u7684\u914d\u7f6e\u5668\u7248\u672c\u4e0d\u652f\u6301\u540e\u53f0\u90e8\u7f72\u3002","download.openhome":"\u6309\u4efb\u610f\u952e\u6253\u5f00\u9879\u76ee\u4e3b\u9875\u5e76\u9000\u51fa\uff1b10 \u79d2\u5185\u65e0\u64cd\u4f5c\u5c06\u4f1a\u81ea\u52a8\u6253\u5f00\u5e76\u9000\u51fa\u3002","download.operationargs":"\u53c2\u6570\u4e0e\u64cd\u4f5c\u4e0d\u5339\u914d\u3002\u8bf7\u68c0\u67e5 Request\u3001Manifest\u3001Id \u548c Replace\u3002","download.redirected":"\u4e34\u65f6\u76ee\u5f55\u88ab\u91cd\u5b9a\u5411\uff0c\u5df2\u8df3\u8fc7\u6e05\u7406\uff1a{path}","download.releasetag":"\u7248\u672c\u6807\u8bb0\u683c\u5f0f\u5f02\u5e38\uff0c\u672a\u542f\u52a8\u914d\u7f6e\u5668\u3002","download.removefile":"\u65e0\u6cd5\u5220\u9664\u4e34\u65f6\u6587\u4ef6\uff1a{path}","download.requestsize":"Request \u6587\u4ef6\u4e0d\u5b58\u5728\u6216\u8d85\u8fc7\u5927\u5c0f\u9650\u5236\u3002","download.resultpath":"Result \u5fc5\u987b\u6307\u5411\u4e00\u4e2a\u5c1a\u4e0d\u5b58\u5728\u7684 .json \u6587\u4ef6\u3002","download.resultsize":"\u914d\u7f6e\u5668\u7684\u7ed3\u679c\u6587\u4ef6\u7f3a\u5931\u6216\u8d85\u8fc7\u5927\u5c0f\u9650\u5236\u3002","download.samepath":"Result \u4e0d\u80fd\u4e0e Request \u6216 Manifest \u6307\u5411\u540c\u4e00\u4e2a\u6587\u4ef6\u3002","download.star":"\u559c\u6b22\u672c\u9879\u76ee\u8bf7\u7ed9\u672c\u9879\u76ee\u70b9\u4e00\u4e2a Star \u8c22\u8c22\u55b5~","download.transfer":"\u6b63\u5728\u4e0b\u8f7d\u914d\u7f6e\u5668","download.uicapability":"\u914d\u7f6e\u5668\u754c\u9762\u4fe1\u606f\u65e0\u6548\uff0c\u672a\u542f\u52a8\u7a0b\u5e8f\u3002","download.uichoice":"\u8f93\u5165 1\u30012 \u6216 0\uff08\u76f4\u63a5\u56de\u8f66\u53d6\u6d88\uff09","download.verify":"\u6b63\u5728\u6821\u9a8c\u6587\u4ef6","download.version":"\u6b63\u5728\u4e0b\u8f7d ChatGPT \u914d\u7f6e\u5668 {version}\u2026","download.webviewfailed":"WebView2 \u5b89\u88c5\u540e\u4ecd\u4e0d\u53ef\u7528\uff08\u9000\u51fa\u7801 {code}\uff09\u3002\u8bf7\u91cd\u65b0\u8fd0\u884c\u547d\u4ee4\u5e76\u9009\u62e9\u517c\u5bb9\u754c\u9762\uff0c\u6216\u624b\u52a8\u5b89\u88c5 WebView2\u3002","download.webviewinstall":"\u5b89\u88c5 WebView2\uff08\u63a8\u8350\uff09","download.webviewinstalling":"\u6b63\u5728\u5b89\u88c5 WebView2","download.webviewmissing":"\u672a\u68c0\u6d4b\u5230 WebView2\u3002\u8bf7\u9009\u62e9\u6253\u5f00\u65b9\u5f0f\uff1a","download.webviewsignature":"\u5b89\u88c5\u5668\u672a\u901a\u8fc7\u5fae\u8f6f\u7b7e\u540d\u6821\u9a8c\uff0c\u672a\u6267\u884c\u5b89\u88c5\u3002\u8bf7\u91cd\u65b0\u8fd0\u884c\u547d\u4ee4\uff0c\u4e5f\u53ef\u9009\u62e9\u517c\u5bb9\u754c\u9762\u3002","download.webviewtransfer":"\u6b63\u5728\u4e0b\u8f7d\u5fae\u8f6f WebView2 \u5b89\u88c5\u5668"},"en-US":{"download.activity":"Preparing ChatGPT Launcher Setup","download.browsererror":"Could not open the browser. Open this address manually: {url}","download.cancel":"Cancel","download.cancelled":"Cancelled. Setup was not opened.","download.checksum":"Downloading checksums","download.cleaned":"Temporary files have been removed.","download.close":"To exit without opening it, close this window.","download.compatibility":"Use compatibility mode","download.graphicalfailed":"The setup tool exited with error code {code}.","download.headless":"Parameters require -Headless.","download.incompletecleanup":"Temporary download files could not be fully removed.","download.incompleteresult":"The setup tool returned an incomplete result.","download.inconsistentresult":"The setup tool\u0027s result does not match its exit code.","download.invalidbranch":"The branch name is invalid or its commit could not be verified.","download.invalidchecksum":"Missing or invalid setup checksum. Setup was not started.","download.invalidresult":"The setup tool returned an invalid result.","download.keepfolder":"Temporary folder retained: {path}","download.latest":"Looking up the release","download.managedresult":"Result cannot be inside the managed directory.","download.manifestsize":"The Manifest file is missing or exceeds the size limit.","download.mismatch":"SHA-256 mismatch. Setup was not started.","download.norelease":"No published release matches the selected channel, branch and tag.","download.oldversion":"Downloaded setup does not support headless deployment.","download.openhome":"Press any key to open the project page and exit, or wait 10 seconds to open it automatically.","download.operationargs":"Parameters do not match the operation. Check Request, Manifest, Id and Replace.","download.redirected":"The temporary folder was redirected; cleanup was skipped: {path}","download.releasetag":"Unexpected release tag. Setup was not started.","download.removefile":"Could not remove temporary file: {path}","download.requestsize":"The Request file is missing or exceeds the size limit.","download.resultpath":"Result must be a new .json file.","download.resultsize":"The setup result file is missing or exceeds the size limit.","download.samepath":"Result must differ from Request and Manifest.","download.star":"If you like this project, please give it a Star. Thank you, meow~","download.transfer":"Downloading the setup tool","download.uicapability":"Invalid setup UI information. Setup was not started.","download.uichoice":"Enter 1, 2 or 0 (Enter to cancel)","download.verify":"Verifying the download","download.version":"Downloading ChatGPT Launcher Setup {version}\u2026","download.webviewfailed":"WebView2 is still unavailable after installation (exit code {code}). Run the command again and choose compatibility mode, or install WebView2 manually.","download.webviewinstall":"Install WebView2 (recommended)","download.webviewinstalling":"Installing WebView2","download.webviewmissing":"WebView2 was not found. Choose how to open setup:","download.webviewsignature":"The installer failed Microsoft signature verification and was not run. Run the command again, or choose compatibility mode.","download.webviewtransfer":"Downloading the Microsoft WebView2 installer"},"ja-JP":{"download.activity":"ChatGPT \u8a2d\u5b9a\u30c4\u30fc\u30eb\u3092\u6e96\u5099\u4e2d","download.browsererror":"\u30d6\u30e9\u30a6\u30b6\u30fc\u3092\u958b\u3051\u307e\u305b\u3093\u3067\u3057\u305f\u3002\u6b21\u306e URL \u3092\u624b\u52d5\u3067\u958b\u3044\u3066\u304f\u3060\u3055\u3044\uff1a{url}","download.cancel":"\u30ad\u30e3\u30f3\u30bb\u30eb","download.cancelled":"\u30ad\u30e3\u30f3\u30bb\u30eb\u3057\u307e\u3057\u305f\u3002\u8a2d\u5b9a\u30c4\u30fc\u30eb\u306f\u8d77\u52d5\u3057\u3066\u3044\u307e\u305b\u3093\u3002","download.checksum":"\u30c1\u30a7\u30c3\u30af\u30b5\u30e0\u3092\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9\u4e2d","download.cleaned":"\u4e00\u6642\u30d5\u30a1\u30a4\u30eb\u3092\u524a\u9664\u3057\u307e\u3057\u305f\u3002","download.close":"\u30da\u30fc\u30b8\u3092\u958b\u304b\u305a\u306b\u7d42\u4e86\u3059\u308b\u5834\u5408\u306f\u3001\u3053\u306e\u30a6\u30a3\u30f3\u30c9\u30a6\u3092\u9589\u3058\u3066\u304f\u3060\u3055\u3044\u3002","download.compatibility":"\u4e92\u63db\u30e2\u30fc\u30c9\u3092\u4f7f\u7528","download.graphicalfailed":"\u8a2d\u5b9a\u30c4\u30fc\u30eb\u304c\u30a8\u30e9\u30fc\u3067\u7d42\u4e86\u3057\u307e\u3057\u305f\u3002\u7d42\u4e86\u30b3\u30fc\u30c9\u306f {code} \u3067\u3059\u3002","download.headless":"\u3053\u308c\u3089\u306e\u30d1\u30e9\u30e1\u30fc\u30bf\u30fc\u306b\u306f -Headless \u304c\u5fc5\u8981\u3067\u3059\u3002","download.incompletecleanup":"\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9\u3057\u305f\u4e00\u6642\u30d5\u30a1\u30a4\u30eb\u3092\u3059\u3079\u3066\u524a\u9664\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\u3002","download.incompleteresult":"\u8a2d\u5b9a\u30c4\u30fc\u30eb\u304c\u8fd4\u3057\u305f\u7d50\u679c\u306b\u4e0d\u8db3\u304c\u3042\u308a\u307e\u3059\u3002","download.inconsistentresult":"\u8a2d\u5b9a\u30c4\u30fc\u30eb\u306e\u72b6\u614b\u3068\u7d42\u4e86\u30b3\u30fc\u30c9\u304c\u4e00\u81f4\u3057\u307e\u305b\u3093\u3002","download.invalidbranch":"\u30d6\u30e9\u30f3\u30c1\u540d\u304c\u7121\u52b9\u304b\u3001\u5bfe\u8c61\u306e\u30b3\u30df\u30c3\u30c8\u3092\u78ba\u8a8d\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\u3002","download.invalidchecksum":"\u8a2d\u5b9a\u30c4\u30fc\u30eb\u306e\u30c1\u30a7\u30c3\u30af\u30b5\u30e0\u304c\u306a\u3044\u304b\u7121\u52b9\u306a\u305f\u3081\u3001\u8d77\u52d5\u3057\u307e\u305b\u3093\u3067\u3057\u305f\u3002","download.invalidresult":"\u8a2d\u5b9a\u30c4\u30fc\u30eb\u304c\u8fd4\u3057\u305f\u7d50\u679c\u306f\u7121\u52b9\u3067\u3059\u3002","download.keepfolder":"\u4e00\u6642\u30d5\u30a9\u30eb\u30c0\u30fc\u3092\u6b8b\u3057\u307e\u3057\u305f\uff1a{path}","download.latest":"\u30ea\u30ea\u30fc\u30b9\u3092\u78ba\u8a8d\u4e2d","download.managedresult":"Result \u3092\u672c\u30c4\u30fc\u30eb\u306e\u7ba1\u7406\u30d5\u30a9\u30eb\u30c0\u30fc\u5185\u306b\u4fdd\u5b58\u3059\u308b\u3053\u3068\u306f\u3067\u304d\u307e\u305b\u3093\u3002","download.manifestsize":"Manifest \u30d5\u30a1\u30a4\u30eb\u304c\u5b58\u5728\u3057\u306a\u3044\u304b\u3001\u30b5\u30a4\u30ba\u5236\u9650\u3092\u8d85\u3048\u3066\u3044\u307e\u3059\u3002","download.mismatch":"SHA-256 \u304c\u4e00\u81f4\u3057\u306a\u3044\u305f\u3081\u3001\u8a2d\u5b9a\u30c4\u30fc\u30eb\u3092\u8d77\u52d5\u3057\u307e\u305b\u3093\u3067\u3057\u305f\u3002","download.norelease":"\u6307\u5b9a\u3057\u305f\u30c1\u30e3\u30f3\u30cd\u30eb\u3001\u30d6\u30e9\u30f3\u30c1\u3001\u30bf\u30b0\u306b\u4e00\u81f4\u3059\u308b\u516c\u958b\u6e08\u307f\u30ea\u30ea\u30fc\u30b9\u306f\u3042\u308a\u307e\u305b\u3093\u3002","download.oldversion":"\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9\u3057\u305f\u8a2d\u5b9a\u30c4\u30fc\u30eb\u306f\u30d0\u30c3\u30af\u30b0\u30e9\u30a6\u30f3\u30c9\u3067\u306e\u914d\u7f6e\u306b\u5bfe\u5fdc\u3057\u3066\u3044\u307e\u305b\u3093\u3002","download.openhome":"\u4f55\u304b\u30ad\u30fc\u3092\u62bc\u3059\u3068\u30d7\u30ed\u30b8\u30a7\u30af\u30c8\u306e\u30da\u30fc\u30b8\u3092\u958b\u3044\u3066\u7d42\u4e86\u3057\u307e\u3059\u300210 \u79d2\u5f8c\u306b\u81ea\u52d5\u3067\u958b\u3044\u3066\u7d42\u4e86\u3057\u307e\u3059\u3002","download.operationargs":"\u30d1\u30e9\u30e1\u30fc\u30bf\u30fc\u304c\u64cd\u4f5c\u306b\u5bfe\u5fdc\u3057\u3066\u3044\u307e\u305b\u3093\u3002Request\u3001Manifest\u3001Id\u3001Replace \u3092\u78ba\u8a8d\u3057\u3066\u304f\u3060\u3055\u3044\u3002","download.redirected":"\u4e00\u6642\u30d5\u30a9\u30eb\u30c0\u30fc\u304c\u5225\u306e\u5834\u6240\u306b\u8ee2\u9001\u3055\u308c\u305f\u305f\u3081\u3001\u524a\u9664\u3092\u884c\u3044\u307e\u305b\u3093\u3067\u3057\u305f\uff1a{path}","download.releasetag":"\u30ea\u30ea\u30fc\u30b9\u30bf\u30b0\u306e\u5f62\u5f0f\u304c\u4e0d\u6b63\u306a\u305f\u3081\u3001\u8a2d\u5b9a\u30c4\u30fc\u30eb\u3092\u8d77\u52d5\u3057\u307e\u305b\u3093\u3067\u3057\u305f\u3002","download.removefile":"\u4e00\u6642\u30d5\u30a1\u30a4\u30eb\u3092\u524a\u9664\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\uff1a{path}","download.requestsize":"Request \u30d5\u30a1\u30a4\u30eb\u304c\u5b58\u5728\u3057\u306a\u3044\u304b\u3001\u30b5\u30a4\u30ba\u5236\u9650\u3092\u8d85\u3048\u3066\u3044\u307e\u3059\u3002","download.resultpath":"Result \u306b\u306f\u3001\u307e\u3060\u5b58\u5728\u3057\u306a\u3044 .json \u30d5\u30a1\u30a4\u30eb\u3092\u6307\u5b9a\u3057\u3066\u304f\u3060\u3055\u3044\u3002","download.resultsize":"\u8a2d\u5b9a\u30c4\u30fc\u30eb\u306e\u7d50\u679c\u30d5\u30a1\u30a4\u30eb\u304c\u306a\u3044\u304b\u3001\u30b5\u30a4\u30ba\u5236\u9650\u3092\u8d85\u3048\u3066\u3044\u307e\u3059\u3002","download.samepath":"Result \u306b\u306f Request \u3084 Manifest \u3068\u7570\u306a\u308b\u30d5\u30a1\u30a4\u30eb\u3092\u6307\u5b9a\u3057\u3066\u304f\u3060\u3055\u3044\u3002","download.star":"\u3053\u306e\u30d7\u30ed\u30b8\u30a7\u30af\u30c8\u304c\u6c17\u306b\u5165\u3063\u305f\u3089\u3001Star \u3092\u304a\u9858\u3044\u3057\u307e\u3059\u3002\u3042\u308a\u304c\u3068\u3046\u306b\u3083\u3093\uff5e","download.transfer":"\u8a2d\u5b9a\u30c4\u30fc\u30eb\u3092\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9\u4e2d","download.uicapability":"\u8a2d\u5b9a\u30c4\u30fc\u30eb\u306e UI \u60c5\u5831\u304c\u7121\u52b9\u306a\u305f\u3081\u3001\u8d77\u52d5\u3057\u307e\u305b\u3093\u3067\u3057\u305f\u3002","download.uichoice":"1\u30012\u30010 \u3092\u5165\u529b\uff08Enter \u3067\u30ad\u30e3\u30f3\u30bb\u30eb\uff09","download.verify":"\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9\u3057\u305f\u30d5\u30a1\u30a4\u30eb\u3092\u691c\u8a3c\u4e2d","download.version":"ChatGPT \u8a2d\u5b9a\u30c4\u30fc\u30eb {version} \u3092\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9\u4e2d\u2026","download.webviewfailed":"\u30a4\u30f3\u30b9\u30c8\u30fc\u30eb\u5f8c\u3082 WebView2 \u3092\u5229\u7528\u3067\u304d\u307e\u305b\u3093\uff08\u7d42\u4e86\u30b3\u30fc\u30c9 {code}\uff09\u3002\u30b3\u30de\u30f3\u30c9\u3092\u518d\u5b9f\u884c\u3057\u3066\u4e92\u63db\u30e2\u30fc\u30c9\u3092\u9078\u3076\u304b\u3001WebView2 \u3092\u624b\u52d5\u3067\u30a4\u30f3\u30b9\u30c8\u30fc\u30eb\u3057\u3066\u304f\u3060\u3055\u3044\u3002","download.webviewinstall":"WebView2 \u3092\u30a4\u30f3\u30b9\u30c8\u30fc\u30eb\uff08\u63a8\u5968\uff09","download.webviewinstalling":"WebView2 \u3092\u30a4\u30f3\u30b9\u30c8\u30fc\u30eb\u4e2d","download.webviewmissing":"WebView2 \u304c\u898b\u3064\u304b\u308a\u307e\u305b\u3093\u3002\u8d77\u52d5\u65b9\u6cd5\u3092\u9078\u629e\u3057\u3066\u304f\u3060\u3055\u3044\uff1a","download.webviewsignature":"Microsoft \u306e\u7f72\u540d\u3092\u78ba\u8a8d\u3067\u304d\u306a\u304b\u3063\u305f\u305f\u3081\u3001\u30a4\u30f3\u30b9\u30c8\u30fc\u30e9\u30fc\u3092\u5b9f\u884c\u3057\u307e\u305b\u3093\u3067\u3057\u305f\u3002\u30b3\u30de\u30f3\u30c9\u3092\u518d\u5b9f\u884c\u3059\u308b\u304b\u3001\u4e92\u63db\u30e2\u30fc\u30c9\u3092\u9078\u629e\u3057\u3066\u304f\u3060\u3055\u3044\u3002","download.webviewtransfer":"Microsoft WebView2 \u30a4\u30f3\u30b9\u30c8\u30fc\u30e9\u30fc\u3092\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9\u4e2d"}}
'@
    # END GENERATED LOCALES
    function Get-SetupLanguage {
        try { $preference = Get-ItemPropertyValue -LiteralPath 'HKCU:\Software\Community\ChatGPTLauncherSetup' -Name Language -ErrorAction Stop } catch { $preference = '' }
        if ($preference -and $null -ne $setupLocales.PSObject.Properties[[string]$preference]) { return [string]$preference }
        try { $candidate = (Get-UICulture).Name } catch { $candidate = '' }
        if (!$candidate) { $candidate = [Globalization.CultureInfo]::InstalledUICulture.Name }
        if ($candidate) {
            if ($null -ne $setupLocales.PSObject.Properties[$candidate]) { return $candidate }
            $primary = $candidate.Split('-')[0]
            foreach ($tag in $setupLocales.PSObject.Properties.Name) { if ($tag.Split('-')[0] -ieq $primary) { return $tag } }
        }
        return 'zh-CN'
    }
    function Get-SetupText([string]$Key, [hashtable]$Values = @{}) {
        $pattern = $setupLocales.$setupLanguage.$Key
        # Replace once; braces in paths or other caller values stay literal.
        return [regex]::Replace($pattern, '\{([a-z][a-zA-Z0-9_]*)\}', [Text.RegularExpressions.MatchEvaluator]{
            param($match)
            $name = $match.Groups[1].Value
            if ($Values.ContainsKey($name)) { return [string]$Values[$name] }
            return $match.Value
        })
    }
    function Test-SetupTagName([string]$Name) {
        return $Name.Length -le 128 -and $Name -cmatch '\Av[0-9]+(?:\.[0-9]+){2,3}(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?\z'
    }
    function Assert-SetupReleaseFilters([string]$Branch, [string]$Tag) {
        if ($Tag -and !(Test-SetupTagName $Tag)) { throw (Get-SetupText 'download.releasetag') }
        if ($Branch -and ($Branch.Length -gt 255 -or $Branch -match '[\s\x00-\x1f\x7f~^:?*\[\\]|\.\.|@\{|(^|/)\.|\.lock(/|$)|^[-/]|[./]$|//|^@$')) {
            throw (Get-SetupText 'download.invalidbranch')
        }
    }
    function Get-SetupReleaseTag([string]$Repo, [string]$Channel, [string]$Branch, [string]$Tag) {
        $api = "https://api.github.com/repos/$Repo"
        $stablePattern = '\Av[0-9]+(?:\.[0-9]+){2,3}\z'
        if ($Branch) {
            $branchRef = Invoke-RestMethod -Uri ($api + '/branches/' + [Uri]::EscapeDataString($Branch)) -TimeoutSec 30
            $head = [string]$branchRef.commit.sha
            if ($branchRef.name -cne $Branch -or $head -cnotmatch '\A(?:[0-9a-f]{40}|[0-9a-f]{64})\z') { throw (Get-SetupText 'download.invalidbranch') }
        }
        $latest = !$Tag -and !$Branch -and $Channel -eq 'Stable'
        if ($Tag -or $latest) {
            $uri = if ($Tag) { "$api/releases/tags/$Tag" } else { "$api/releases/latest" }
            $candidates = @(Invoke-RestMethod -Uri $uri -TimeoutSec 30)
        } else {
            $releases = [Collections.Generic.List[object]]::new()
            for ($page = 1; ; $page++) {
                $records = Invoke-RestMethod -Uri "$api/releases?per_page=100&page=$page" -TimeoutSec 30
                foreach ($record in $records) {
                    if ($record.draft -eq $false -and $record.published_at) { $releases.Add($record) }
                }
                if ($records.Count -lt 100) { break }
            }
            $candidates = $releases | Sort-Object @{Expression={[DateTimeOffset]$_.published_at}}, @{Expression={[long]$_.id}} -Descending
        }
        foreach ($release in $candidates) {
            $name = [string]$release.tag_name
            if (!(Test-SetupTagName $name) -or ($Tag -and $Tag -cne $name) -or
                $release.draft -isnot [bool] -or $release.draft -or $release.prerelease -isnot [bool]) { continue }
            if ($Channel -eq 'Stable' -and ($release.prerelease -or $name -cnotmatch $stablePattern)) { continue }
            if ($Branch) {
                # target_commitish can be stale when the release uses an existing tag.
                $comparison = Invoke-RestMethod -Uri ($api + '/compare/' + $name + '...' + $head + '?per_page=1') -TimeoutSec 30
                if ($comparison.status -cin @('diverged', 'behind')) { continue }
                if ($comparison.status -cnotin @('ahead', 'identical')) { throw (Get-SetupText 'download.invalidbranch') }
            }
            return $name
        }
        throw (Get-SetupText 'download.norelease')
    }
    $setupLanguage = Get-SetupLanguage
    # Enable native transfer progress only in this script's scope.
    $ProgressPreference = if ($Headless) { 'SilentlyContinue' } else { 'Continue' }
    $progress = @{ Id = 1; Activity = (Get-SetupText 'download.activity') }
    function Invoke-SetupProgress([scriptblock]$Action) {
        if ($Headless) { & $Action; return }
        $savedView = $null
        # Minimal progress can count CJK characters as one cell and leave wrapped rows behind.
        if ($PSVersionTable.PSVersion -ge [version]'7.2') {
            $savedView = $PSStyle.Progress.View
            $PSStyle.Progress.View = 'Classic'
        }
        try { & $Action }
        finally {
            try { Write-Progress @progress -Completed }
            finally {
                if ($null -ne $savedView) { $PSStyle.Progress.View = $savedView }
            }
        }
    }
    function Show-SetupStage([string]$Status, [switch]$Completed) {
        if (!$Headless) {
            if ($Completed) { Write-Progress @progress -Completed }
            else { Write-Progress @progress -Status $Status }
        }
    }
    function Get-SetupLocalPath([string]$Path) {
        if ($Path -notmatch '\A[A-Za-z]:\\' -or $Path.IndexOfAny([char[]]'"<>|?*/') -ge 0 -or
            $Path -match '[\x00-\x1f]' -or $Path.Substring(2).Contains(':')) { throw 'invalid-local-path' }
        $full = [IO.Path]::GetFullPath($Path)
        foreach ($component in $full.Substring(3).Split('\')) {
            if ($component -match '[. ]$' -or $component -match '\A(?:CON|PRN|AUX|NUL|COM[1-9\u00b9\u00b2\u00b3]|LPT[1-9\u00b9\u00b2\u00b3])(?:\.|$)') { throw 'invalid-path-component' }
        }
        $part = $full
        while ($part) {
            if ([IO.File]::Exists($part) -or [IO.Directory]::Exists($part)) {
                if (([IO.File]::GetAttributes($part) -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'redirected-path' }
            }
            $part = [IO.Path]::GetDirectoryName($part)
        }
        return $full
    }
    function Test-SetupModernUi([string]$Path) {
        # Map resources as data. Never probe an old executable with new arguments.
        if (!('ChatGPTSetup.UiResourceV1' -as [type])) {
            Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace ChatGPTSetup {
    public static class UiResourceV1 {
        [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
        static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);
        [DllImport("kernel32.dll", SetLastError=true)]
        static extern IntPtr FindResourceW(IntPtr module, IntPtr name, IntPtr type);
        [DllImport("kernel32.dll")] static extern uint SizeofResource(IntPtr module, IntPtr resource);
        [DllImport("kernel32.dll")] static extern IntPtr LoadResource(IntPtr module, IntPtr resource);
        [DllImport("kernel32.dll")] static extern IntPtr LockResource(IntPtr resource);
        [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr module);
        public static byte[] Read(string path) {
            IntPtr module = LoadLibraryExW(path, IntPtr.Zero, 0x22);
            if (module == IntPtr.Zero) throw new InvalidOperationException("Cannot read setup resources");
            try {
                IntPtr resource = FindResourceW(module, new IntPtr(102), new IntPtr(10));
                if (resource == IntPtr.Zero) return null;
                uint size = SizeofResource(module, resource);
                if (size == 0 || size > 1024) throw new InvalidOperationException("Invalid UI resource size");
                IntPtr data = LockResource(LoadResource(module, resource));
                if (data == IntPtr.Zero) throw new InvalidOperationException("Cannot read UI resource");
                byte[] bytes = new byte[size];
                Marshal.Copy(data, bytes, 0, bytes.Length);
                return bytes;
            } finally { FreeLibrary(module); }
        }
    }
}
'@
        }
        $bytes = [ChatGPTSetup.UiResourceV1]::Read($Path)
        if ($null -eq $bytes) { return $false }
        $capability = ([Text.UTF8Encoding]::new($false, $true)).GetString($bytes) | ConvertFrom-Json
        if ($capability.schemaVersion -ne 1 -or $capability.requiresWebView2 -isnot [bool] -or
            !$capability.requiresWebView2 -or $capability.compatibilityArgument -cne '--compatibility') {
            throw (Get-SetupText 'download.uicapability')
        }
        return $true
    }
    function Get-SetupRuntimeVersion([Microsoft.Win32.RegistryHive]$Hive, [Microsoft.Win32.RegistryView]$View) {
        $baseKey = $null; $key = $null
        try {
            $baseKey = [Microsoft.Win32.RegistryKey]::OpenBaseKey($Hive, $View)
            $key = $baseKey.OpenSubKey('SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}')
            if ($key) { return $key.GetValue('pv') }
        } finally {
            if ($key) { $key.Dispose() }
            if ($baseKey) { $baseKey.Dispose() }
        }
    }
    function Test-SetupWebView2 {
        foreach ($hive in @('LocalMachine', 'CurrentUser')) {
            foreach ($view in @('Registry32', 'Registry64')) {
                $version = $null
                try { $value = Get-SetupRuntimeVersion $hive $view }
                catch [System.Security.SecurityException] { continue }
                catch [UnauthorizedAccessException] { continue }
                catch [IO.IOException] { continue }
                if ($value -is [string] -and [version]::TryParse($value, [ref]$version) -and $version -gt [version]'0.0.0.0') { return $true }
            }
        }
        return $false
    }
    function Select-SetupUi {
        Write-Host (Get-SetupText 'download.webviewmissing')
        Write-Host ('1. ' + (Get-SetupText 'download.webviewinstall'))
        Write-Host ('2. ' + (Get-SetupText 'download.compatibility'))
        Write-Host ('0. ' + (Get-SetupText 'download.cancel'))
        do {
            $answer = Read-Host (Get-SetupText 'download.uichoice')
        } while ($answer -notin @('1', '2', '0', ''))
        switch ($answer) { '1' { return 'install' }; '2' { return 'compatibility' }; default { return 'cancel' } }
    }
    function Install-SetupWebView2([string]$Installer) {
        # Reserve the name only after the user selected installation. A pre-existing file is foreign.
        [IO.File]::Open($Installer, 'CreateNew', 'Write', 'None').Dispose()
        try {
            Invoke-SetupProgress {
                Show-SetupStage (Get-SetupText 'download.webviewtransfer')
                Invoke-WebRequest -UseBasicParsing -Uri 'https://go.microsoft.com/fwlink/p/?LinkId=2124703' -OutFile $Installer -TimeoutSec 120
                $stream = [IO.File]::OpenRead($Installer)
                try {
                    $signature = Get-AuthenticodeSignature -LiteralPath $Installer
                    if ($signature.Status -ne 'Valid' -or !$signature.SignerCertificate -or
                        $signature.SignerCertificate.Subject -notmatch '(?:^|,\s*)O=Microsoft Corporation(?:,|$)') {
                        throw (Get-SetupText 'download.webviewsignature')
                    }
                    Show-SetupStage (Get-SetupText 'download.webviewinstalling')
                    $process = Start-Process -FilePath $Installer -ArgumentList @('/silent', '/install') -WindowStyle Hidden -PassThru
                    try { $process.WaitForExit(); $code = $process.ExitCode } finally { $process.Dispose() }
                    if ($code -notin @(0, 3010) -or !(Test-SetupWebView2)) { throw (Get-SetupText 'download.webviewfailed' @{code=$code}) }
                } finally { $stream.Dispose() }
            }
        } finally {
            $directory = [IO.Path]::GetDirectoryName($Installer)
            try {
                if (([IO.File]::GetAttributes($directory) -band [IO.FileAttributes]::ReparsePoint) -eq 0) {
                    [IO.File]::Delete($Installer)
                }
            } catch { Write-Warning (Get-SetupText 'download.removefile' @{path=$Installer}) }
        }
    }
    function Get-SetupUiMode([string]$Path, [string]$Installer) {
        if (!(Test-SetupModernUi $Path) -or (Test-SetupWebView2)) { return 'default' }
        $choice = Select-SetupUi
        if ($choice -eq 'install') {
            Install-SetupWebView2 $Installer
            return 'default'
        }
        return $choice
    }
    function Start-SetupGraphical([string]$Path, [string]$Folder, [string]$Installer) {
        $mode = Get-SetupUiMode $Path $Installer
        if ($mode -eq 'cancel') { return }
        if ($mode -eq 'compatibility') {
            return Start-Process -FilePath $Path -WorkingDirectory $Folder -ArgumentList '--compatibility' -PassThru
        }
        return Start-Process -FilePath $Path -WorkingDirectory $Folder -PassThru
    }
    function Read-SetupKey {
        # Redirected input and non-console hosts may not support keyboard input.
        try {
            if (![Console]::IsInputRedirected -and [Console]::KeyAvailable) {
                return [Console]::ReadKey($true)
            }
        } catch { }
        return $null
    }
    function Show-SetupCompletion {
        Write-Host ''
        # The user may have changed language in the setup window.
        $setupLanguage = Get-SetupLanguage
        Write-Host ((Get-SetupText 'download.cleaned'))
        Write-Host ((Get-SetupText 'download.star')) -ForegroundColor Cyan
        $url = 'https://github.com/EthenGod/ChatGPT-Proxy-Launcher'
        Write-Host $url
        Write-Host ((Get-SetupText 'download.openhome'))
        Write-Host ((Get-SetupText 'download.close'))
        for ($tick = 0; $tick -lt 100; $tick++) {
            if ($null -ne (Read-SetupKey)) { break }
            Start-Sleep -Milliseconds 100
        }
        try { Start-Process -FilePath $url }
        catch { Write-Warning (Get-SetupText 'download.browsererror' @{url=$url}) }
    }
    $repo = 'EthenGod/ChatGPT-Proxy-Launcher'
    $folder = $null; $exe = $null; $sums = $null; $nativeResult = $null; $resultStream = $null; $webviewInstaller = $null
    $started = $false; $ownedFolder = $false; $cleanupComplete = $true; $phase = 'parse'; $selectedTag = $null
    $outcome = [ordered]@{schemaVersion=1; operation=$Operation; status='error'; exitCode=20; code='invalid-request';
        committed=$false; stage='parse'; nativeError=0; warnings=@(); actionsRequired=@(); data=@{}}
    try {
        if ($Request -or $Result -or $Manifest -or $Id -or $Replace -or $Operation -ne 'deploy') {
            if (!$Headless) { throw (Get-SetupText 'download.headless') }
        }
        if ($Headless) {
            $Result = Get-SetupLocalPath $Result
            if ([IO.Path]::GetExtension($Result) -ine '.json') { throw (Get-SetupText 'download.resultpath') }
            $managedRoot = [IO.Path]::Combine([Environment]::GetFolderPath('LocalApplicationData'), 'ChatGPTProxyLauncher') + '\'
            if ($Result.StartsWith($managedRoot, [StringComparison]::OrdinalIgnoreCase)) { throw (Get-SetupText 'download.managedresult') }
            $needsRequest = $Operation -in @('deploy', 'plan', 'companion-configure')
            if ($needsRequest) { $Request = Get-SetupLocalPath $Request }
            elseif ($Request) { throw (Get-SetupText 'download.operationargs') }
            if ($Operation -eq 'companion-register') { $Manifest = Get-SetupLocalPath $Manifest }
            elseif ($Manifest -or $Replace) { throw (Get-SetupText 'download.operationargs') }
            if ($Operation -eq 'companion-remove') {
                # Keep an ID a single native argument; the EXE performs final ownership validation.
                if (!$Id -or $Id.Length -gt 128 -or !$Id.Contains('.') -or
                    $Id -cnotmatch '\A[a-z][a-z0-9]*(?:[.-][a-z0-9]+)*\z') { throw (Get-SetupText 'download.operationargs') }
            } elseif ($Id) { throw (Get-SetupText 'download.operationargs') }
            if (($Request -and $Request -ieq $Result) -or ($Manifest -and $Manifest -ieq $Result)) { throw (Get-SetupText 'download.samepath') }
            if ($Request -and (![IO.File]::Exists($Request) -or (Get-Item -LiteralPath $Request).Length -gt 65536)) { throw (Get-SetupText 'download.requestsize') }
            if ($Manifest -and (![IO.File]::Exists($Manifest) -or (Get-Item -LiteralPath $Manifest).Length -gt 65536)) { throw (Get-SetupText 'download.manifestsize') }
            $phase = 'result'
            $resultStream = [IO.File]::Open($Result, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
            $phase = 'parse'
        }
        if ($entryParameters -and $entryParameters.ContainsKey('Branch') -and !$Branch) { throw (Get-SetupText 'download.invalidbranch') }
        if ($entryParameters -and $entryParameters.ContainsKey('Tag') -and !$Tag) { throw (Get-SetupText 'download.releasetag') }
        Assert-SetupReleaseFilters $Branch $Tag
        $phase = 'transport'
        $selectedTag = Invoke-SetupProgress {
            Show-SetupStage ((Get-SetupText 'download.latest'))
            Get-SetupReleaseTag $repo $Channel $Branch $Tag
        }
        $base = "https://github.com/$repo/releases/download/$selectedTag"
        $name = 'ChatGPTLauncherSetup.exe'
        $folder = Join-Path $env:TEMP ('ChatGPTLauncherSetup-' + [guid]::NewGuid().ToString('N'))
        $null = New-Item -ItemType Directory -Path $folder
        $ownedFolder = $true
        $sums = Join-Path $folder 'SHA256SUMS.txt'
        $exe = Join-Path $folder $name
        $nativeResult = Join-Path $folder 'native-result.json'
        $webviewInstaller = Join-Path $folder 'MicrosoftEdgeWebview2Setup.exe'
        if (!$Headless) { Write-Host (Get-SetupText 'download.version' @{version=$selectedTag}) }
        $setupProcess = Invoke-SetupProgress {
            Show-SetupStage ((Get-SetupText 'download.checksum'))
            Invoke-WebRequest -UseBasicParsing -Uri "$base/SHA256SUMS.txt" -OutFile $sums -TimeoutSec 60
            $entries = @([IO.File]::ReadAllLines($sums) -cmatch '\s\*?ChatGPTLauncherSetup\.exe$')
            if ($entries.Count -ne 1 -or $entries[0] -cnotmatch '^([a-fA-F0-9]{64})[ \t]+\*?ChatGPTLauncherSetup\.exe$') {
                throw (Get-SetupText 'download.invalidchecksum')
            }
            $expected = $Matches[1]
            Show-SetupStage ((Get-SetupText 'download.transfer'))
            Invoke-WebRequest -UseBasicParsing -Uri "$base/$name" -OutFile $exe -TimeoutSec 120
            Show-SetupStage ((Get-SetupText 'download.verify'))
            $stream = [IO.File]::OpenRead($exe)
            try {
                $sha = [Security.Cryptography.SHA256]::Create()
                try { $actual = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') }
                finally { $sha.Dispose() }
                if ($actual -ine $expected) { throw (Get-SetupText 'download.mismatch') }
                Show-SetupStage -Completed
                if ($Headless) {
                    $version = (Get-Item -LiteralPath $exe).VersionInfo
                    if ($version.FileMajorPart -lt 1 -or ($version.FileMajorPart -eq 1 -and $version.FileMinorPart -lt 1)) { throw (Get-SetupText 'download.oldversion') }
                    $arguments = @('--headless')
                    if ($Operation.StartsWith('companion-')) { $arguments += @('companion', $Operation.Substring(10)) }
                    else { $arguments += $Operation }
                    $arguments += @('--result', ('"' + $nativeResult + '"'))
                    if ($Request) { $arguments += @('--request', ('"' + $Request + '"')) }
                    if ($Manifest) { $arguments += @('--manifest', ('"' + $Manifest + '"')) }
                    if ($Id) { $arguments += @('--id', $Id) }
                    if ($Replace) { $arguments += '--replace' }
                    Start-Process -FilePath $exe -WorkingDirectory $folder -ArgumentList $arguments -WindowStyle Hidden -PassThru
                } else { Start-SetupGraphical $exe $folder $webviewInstaller }
            } finally { $stream.Dispose() }
        }
        if (!$setupProcess) { Write-Host (Get-SetupText 'download.cancelled'); return }
        $started = $true
        try { $setupProcess.WaitForExit(); $nativeExit = $setupProcess.ExitCode } finally { $setupProcess.Dispose() }
        if (!$Headless -and $nativeExit -ne 0) { throw (Get-SetupText 'download.graphicalfailed' @{code=$nativeExit}) }
        if ($Headless) {
            if (![IO.File]::Exists($nativeResult) -or (Get-Item -LiteralPath $nativeResult).Length -gt 1MB) { throw (Get-SetupText 'download.resultsize') }
            $utf8 = New-Object Text.UTF8Encoding($false, $true)
            $native = $utf8.GetString([IO.File]::ReadAllBytes($nativeResult)) | ConvertFrom-Json
            if ($native.schemaVersion -ne 1 -or $native.operation -cne $Operation -or $native.exitCode -ne $nativeExit -or
                $native.status -cnotin @('success','success-with-warnings','error') -or
                $null -eq $native.PSObject.Properties['committed'] -or !$native.code) { throw (Get-SetupText 'download.invalidresult') }
            if (($null -ne $native.committed -and $native.committed -isnot [bool]) -or
                $native.exitCode -notin @(0,10,20,21,30,31,40,41,50,70) -or
                ($native.exitCode -eq 0 -and $native.status -cne 'success') -or
                ($native.exitCode -eq 10 -and $native.status -cne 'success-with-warnings') -or
                ($native.exitCode -gt 10 -and $native.status -cne 'error')) { throw (Get-SetupText 'download.inconsistentresult') }
            foreach ($key in @('schemaVersion','operation','status','exitCode','code','committed','stage','nativeError','warnings','actionsRequired','data')) {
                if ($null -eq $native.PSObject.Properties[$key]) { throw (Get-SetupText 'download.incompleteresult') }
                $outcome[$key] = $native.$key
            }
        }
    } catch {
        if (!$Headless) { throw }
        $outcome.status = 'error'; $outcome.stage = $phase
        $outcome.exitCode = if ($started) { 72 } elseif ($phase -eq 'result') { 70 } elseif ($phase -eq 'parse') { 20 } else { 71 }
        $outcome.code = if ($started) { 'native-result-unavailable' } elseif ($phase -eq 'result') { 'result-unavailable' } elseif ($phase -eq 'parse') { 'invalid-request' } else { 'download-failed' }
        $outcome.committed = if ($started) { $null } else { $false }
    } finally {
        # Only the downloads and headless result are ours. Do not remove other contents.
        if ($ownedFolder -and $folder -and [IO.Directory]::Exists($folder)) {
            if (([IO.File]::GetAttributes($folder) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                $cleanupComplete = $false
                if (!$Headless) { Write-Warning (Get-SetupText 'download.redirected' @{path=$folder}) }
            } else {
                try { [IO.File]::Delete($exe) } catch { $cleanupComplete = $false; if (!$Headless) { Write-Warning (Get-SetupText 'download.removefile' @{path=$exe}) } }
                try { [IO.File]::Delete($sums) } catch { $cleanupComplete = $false; if (!$Headless) { Write-Warning (Get-SetupText 'download.removefile' @{path=$sums}) } }
                if ($Headless -and $nativeResult) { try { [IO.File]::Delete($nativeResult) } catch { $cleanupComplete = $false } }
                try { [IO.Directory]::Delete($folder, $false) } catch { $cleanupComplete = $false; if (!$Headless) { Write-Warning (Get-SetupText 'download.keepfolder' @{path=$folder}) } }
            }
        }
    }
    if ($Headless) {
        $outcome.transport = @{ releaseTag=$selectedTag; cleanupComplete=$cleanupComplete }
        if (!$cleanupComplete) {
            $outcome.warnings = @($outcome.warnings) + @(@{code='cleanup-incomplete'; message=(Get-SetupText 'download.incompletecleanup')})
            if ($outcome.exitCode -in @(0,10)) { $outcome.exitCode=10; $outcome.status='success-with-warnings'; $outcome.code='cleanup-incomplete' }
        }
        if ($resultStream) {
            try {
                $utf8 = New-Object Text.UTF8Encoding($false)
                $bytes = $utf8.GetBytes(($outcome | ConvertTo-Json -Depth 20))
                $resultStream.Write($bytes, 0, $bytes.Length); $resultStream.Flush($true)
            } catch { $outcome.exitCode=70; $outcome.status='error'; $outcome.code='result-unavailable'; $outcome.stage='result' }
            finally { $resultStream.Dispose() }
        }
        return [pscustomobject]$outcome
    }
    if ($cleanupComplete) {
        Show-SetupCompletion
        # With the documented irm | iex command, exit only this PowerShell session.
        exit 0
    }
} $PSBoundParameters
