using System;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Linq;
using System.Collections.Generic;
using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Win32;

// Runs only when the viewer's Help / Check for updates command is clicked.
internal sealed class UpdateClient : Form {
    readonly Label status = new Label { Left=20, Top=20, Width=480, Height=65 };
    readonly ProgressBar progress = new ProgressBar { Left=20, Top=90, Width=480, Style=ProgressBarStyle.Marquee };
    readonly Button close = new Button { Text="关闭", Left=410, Top=130, Width=90 };
    readonly string exe, repo;
    bool installing;
    const long MaxInstaller = 128L*1024*1024;
    internal static Version ParseVersion(string text) {
        if(!Regex.IsMatch(text ?? "", @"^v?\d+\.\d+\.\d+\.\d+$")) throw new InvalidDataException("发布版本号必须为 v1.0.0.27 格式。");
        return new Version(text.TrimStart('v'));
    }
    internal static Uri Trusted(string url) {
        Uri uri;
        if(!Uri.TryCreate(url,UriKind.Absolute,out uri) || uri.Scheme!="https" || !uri.IsDefaultPort || uri.UserInfo!="" ||
           !(new [] {"api.github.com","github.com","release-assets.githubusercontent.com","objects.githubusercontent.com"}).Contains(uri.Host.ToLowerInvariant()))
            throw new InvalidDataException("更新下载地址不受信任。");
        return uri;
    }
    static string Hash(string file) { using(var sha=SHA256.Create()) using(var f=File.OpenRead(file)) return BitConverter.ToString(sha.ComputeHash(f)).Replace("-","").ToLowerInvariant(); }
    static HttpClient Client() {
        var client = new HttpClient(new HttpClientHandler { AllowAutoRedirect=false });
        client.Timeout=TimeSpan.FromMinutes(5);
        client.DefaultRequestHeaders.UserAgent.ParseAdd("fyto-updater/1.0");
        return client;
    }
    static async Task<HttpResponseMessage> Get(HttpClient client,string url) {
        Uri current=Trusted(url);
        for(int i=0;i<6;i++) {
            var response=await client.GetAsync(current,HttpCompletionOption.ResponseHeadersRead);
            int code=(int)response.StatusCode;
            if(code>=300 && code<400) {
                var next=response.Headers.Location;
                response.Dispose();
                if(next==null) throw new IOException("更新服务器返回了无效重定向。");
                current=Trusted(new Uri(current,next).AbsoluteUri); continue;
            }
            if(code==404) { response.Dispose(); throw new IOException("无法访问更新发布。请确认仓库已公开且已发布正式版本。"); }
            response.EnsureSuccessStatusCode();return response;
        }
        throw new IOException("下载重定向次数过多。");
    }
    public UpdateClient(string path,string repository) {
        exe=Path.GetFullPath(path);repo=repository;
        Text="fyto · 检查更新";ClientSize=new System.Drawing.Size(520,180);
        FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;MinimizeBox=false;StartPosition=FormStartPosition.CenterScreen;
        Controls.AddRange(new Control[]{status,progress,close}); close.Click+=(s,e)=>Close();
        FormClosing+=(s,e)=>{if(installing)e.Cancel=true;};
        Shown+=async(s,e)=>await RunUpdate();
    }
    async Task RunUpdate() {
        string download=null;
        try {
            if(!Regex.IsMatch(repo,@"^[A-Za-z0-9][A-Za-z0-9-]*/[A-Za-z0-9_.-]+$")) throw new InvalidDataException("请先配置有效的 GitHub 仓库（所有者/仓库名）。");
            status.Text="正在检查 GitHub 正式版本…";
            Version current=ParseVersion(FileVersionInfo.GetVersionInfo(exe).FileVersion);
            using(var client=Client()) {
                string json;
                using(var response=await Get(client,"https://api.github.com/repos/"+repo+"/releases/latest")) {
                    if(response.Content.Headers.ContentLength>2*1024*1024)throw new InvalidDataException("版本信息过大。");
                    json=await response.Content.ReadAsStringAsync();
                    if(json.Length>2*1024*1024)throw new InvalidDataException("版本信息过大。");
                }
                var release=new JavaScriptSerializer().Deserialize<Dictionary<string,object>>(json);
                if((bool)release["draft"] || (bool)release["prerelease"])throw new InvalidDataException("当前发布不是正式版本。");
                Version latest=ParseVersion((string)release["tag_name"]);
                if(latest<=current){status.Text="当前已是最新版本："+current;return;}
                string name="fyto-Setup-"+latest+".exe";
                var assets=((System.Collections.IEnumerable)release["assets"]).Cast<Dictionary<string,object>>();
                var asset=assets.SingleOrDefault(a=>(string)a["name"]==name);
                if(asset==null || (string)asset["state"]!="uploaded")throw new InvalidDataException("新版缺少安装文件："+name);
                long expected=Convert.ToInt64(asset["size"]);
                if(expected<=0 || expected>MaxInstaller)throw new InvalidDataException("安装包大小异常。");
                object raw;string digest=asset.TryGetValue("digest",out raw)?raw as string:null;
                if(!Regex.IsMatch(digest ?? "",@"^sha256:[a-fA-F0-9]{64}$"))throw new InvalidDataException("发布文件缺少 SHA-256 校验值，请联系发布者重新上传。");
                string url=(string)asset["browser_download_url"];
                if(!url.StartsWith("https://github.com/"+repo+"/releases/download/",StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("安装包不属于指定仓库。");
                download=Path.Combine(Path.GetDirectoryName(Application.ExecutablePath),name);
                status.Text="发现新版 "+latest+"，正在下载并校验…";
                using(var response=await Get(client,url)) using(var input=await response.Content.ReadAsStreamAsync()) using(var output=new FileStream(download,FileMode.CreateNew,FileAccess.Write,FileShare.None)) {
                    var buffer=new byte[65536];long total=0;int count;
                    progress.Style=ProgressBarStyle.Continuous;
                    while((count=await input.ReadAsync(buffer,0,buffer.Length))>0) {
                        total+=count;if(total>expected)throw new InvalidDataException("安装包超出声明大小。");
                        await output.WriteAsync(buffer,0,count);progress.Value=(int)(total*100/expected);
                    }
                    if(total!=expected)throw new InvalidDataException("安装包下载不完整。");
                }
                if(!String.Equals(Hash(download),digest.Substring(7),StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("安装包校验失败，已取消更新。");
                var metadata=FileVersionInfo.GetVersionInfo(download);
                if(metadata.ProductName!="fyto Installer" || ParseVersion(metadata.FileVersion)!=latest)throw new InvalidDataException("安装包名称或版本校验失败。");
                status.Text="正在安装新版，请在 Windows 提示中允许安装。";
                installing=true;close.Enabled=false;
                int result;
                using(var process=Process.Start(new ProcessStartInfo(download,"/update"){UseShellExecute=true,Verb="runas"})) {
                    await Task.Run(()=>process.WaitForExit());result=process.ExitCode;
                }
                installing=false;close.Enabled=true;
                if(result!=0 && result!=3010)throw new IOException("更新安装未完成（代码 "+result+"）。");
                string restart=exe;
                if(ParseVersion(FileVersionInfo.GetVersionInfo(restart).FileVersion)<latest) {
                    using(var key=RegistryKey.OpenBaseKey(RegistryHive.LocalMachine,RegistryView.Registry64).OpenSubKey(@"Software\fyto")) {
                        restart=key==null?"":Path.Combine((string)key.GetValue("InstallLocation",""),"fyto.exe");
                    }
                }
                if(!File.Exists(restart) || ParseVersion(FileVersionInfo.GetVersionInfo(restart).FileVersion)<latest)throw new IOException("安装结束，但未找到已更新的 fyto，请检查安装目录。");
                Process.Start(new ProcessStartInfo(restart,"/setup /window"){UseShellExecute=true});
                Close();
            }
        } catch(Exception e) {installing=false;close.Enabled=true;status.Text="更新未完成："+e.Message;}
        finally {progress.Style=ProgressBarStyle.Continuous;progress.Value=0;if(download!=null)try{File.Delete(download);}catch(IOException){} }
    }
    [STAThread] static int Main(string[] args) {
        if(args.Length==1 && args[0]=="--self-test") {
            if(ParseVersion("v1.0.0.28")<=ParseVersion("1.0.0.27"))return 1;
            foreach(string bad in new[]{"1.0","v1.0.0.28-beta","../1.0.0.28"}){try{ParseVersion(bad);return 2;}catch(InvalidDataException){}}
            foreach(string bad in new[]{"http://github.com/x","https://github.com.evil.test/x","https://user@github.com/x","https://github.com:444/x"}){try{Trusted(bad);return 3;}catch(InvalidDataException){}}
            Trusted("https://release-assets.githubusercontent.com/a");
            var sample=new JavaScriptSerializer().Deserialize<Dictionary<string,object>>("{\"assets\":[{\"name\":\"fyto-Setup-1.0.0.28.exe\"}],\"draft\":false}");
            var asset=((System.Collections.IEnumerable)sample["assets"]).Cast<Dictionary<string,object>>().Single();
            if((string)asset["name"]!="fyto-Setup-1.0.0.28.exe" || (bool)sample["draft"])return 5;
            return 0;
        }
        if(args.Length!=2)return 4;
        bool created;using(var mutex=new System.Threading.Mutex(true,@"Local\fyto-manual-updater",out created)) {
            if(!created)return 0;
            ServicePointManager.SecurityProtocol=SecurityProtocolType.Tls12;
            Application.EnableVisualStyles();Application.Run(new UpdateClient(args[0],args[1]));
        }
        return 0;
    }
}
