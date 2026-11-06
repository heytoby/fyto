using System;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Linq;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Text;
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
    static Dictionary<string,object> FindInstaller(IEnumerable<Dictionary<string,object>> assets, Version version) {
        string prefix="fyto-Setup-"+version;
        return assets.SingleOrDefault(a=>(string)a["name"]==prefix+".msi") ??
               assets.SingleOrDefault(a=>(string)a["name"]==prefix+".exe");
    }

    [DllImport("msi.dll",CharSet=CharSet.Unicode)]
    static extern uint MsiOpenDatabase(string path,IntPtr persist,out uint database);
    [DllImport("msi.dll",CharSet=CharSet.Unicode)]
    static extern uint MsiDatabaseOpenView(uint database,string query,out uint view);
    [DllImport("msi.dll")] static extern uint MsiViewExecute(uint view,uint record);
    [DllImport("msi.dll")] static extern uint MsiViewFetch(uint view,out uint record);
    [DllImport("msi.dll",CharSet=CharSet.Unicode)]
    static extern uint MsiRecordGetString(uint record,uint field,StringBuilder value,ref uint length);
    [DllImport("msi.dll")] static extern uint MsiCloseHandle(uint handle);

    static string ReadMsiProperty(uint database,string property) {
        uint view=0,record=0,length=256;
        var value=new StringBuilder((int)length);
        string query="SELECT `Value` FROM `Property` WHERE `Property`='"+property+"'";
        try {
            if(MsiDatabaseOpenView(database,query,out view)!=0 || MsiViewExecute(view,0)!=0 ||
               MsiViewFetch(view,out record)!=0 || MsiRecordGetString(record,1,value,ref length)!=0)
                throw new InvalidDataException("无法读取 MSI 安装包属性。");
            return value.ToString();
        } finally {
            if(record!=0)MsiCloseHandle(record);
            if(view!=0)MsiCloseHandle(view);
        }
    }

    static void VerifyInstaller(string path,Version version) {
        if(!path.EndsWith(".msi",StringComparison.OrdinalIgnoreCase)) {
            var metadata=FileVersionInfo.GetVersionInfo(path);
            if(metadata.ProductName!="fyto Installer" || ParseVersion(metadata.FileVersion)!=version)
                throw new InvalidDataException("安装包名称或版本校验失败。");
            return;
        }
        uint database;
        if(MsiOpenDatabase(path,IntPtr.Zero,out database)!=0)
            throw new InvalidDataException("无法打开 MSI 安装包。");
        try {
            string expected=String.Format(CultureInfo.InvariantCulture,"{0}.{1}.{2}",version.Major,version.Minor,version.Revision);
            Guid upgradeCode;
            if(version.Build!=0 || ReadMsiProperty(database,"ProductName")!="fyto 中文版" ||
               ReadMsiProperty(database,"ProductVersion")!=expected ||
               !Guid.TryParse(ReadMsiProperty(database,"UpgradeCode"),out upgradeCode) ||
               upgradeCode!=new Guid("A9C96FF0-C5C9-41CD-BF19-E0E825F7E76B"))
                throw new InvalidDataException("MSI 安装包的产品或版本校验失败。");
        } finally { MsiCloseHandle(database); }
    }

    static ProcessStartInfo InstallerCommand(string path) {
        var info=path.EndsWith(".msi",StringComparison.OrdinalIgnoreCase)
            ? new ProcessStartInfo(Path.Combine(Environment.SystemDirectory,"msiexec.exe"),"/i \""+path+"\" /norestart")
            : new ProcessStartInfo(path,"/update");
        info.UseShellExecute=true;
        info.Verb="runas";
        return info;
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
                string tag=(string)release["tag_name"];
                Version latest=ParseVersion(tag);
                if(latest<=current){status.Text="当前已是最新版本："+current;return;}
                var assets=((System.Collections.IEnumerable)release["assets"]).Cast<Dictionary<string,object>>();
                var asset=FindInstaller(assets,latest);
                if(asset==null || (string)asset["state"]!="uploaded")throw new InvalidDataException("新版缺少 Windows 安装包。");
                string name=(string)asset["name"];
                long expected=Convert.ToInt64(asset["size"]);
                if(expected<=0 || expected>MaxInstaller)throw new InvalidDataException("安装包大小异常。");
                object raw;string digest=asset.TryGetValue("digest",out raw)?raw as string:null;
                if(!Regex.IsMatch(digest ?? "",@"^sha256:[a-fA-F0-9]{64}$"))throw new InvalidDataException("发布文件缺少 SHA-256 校验值，请联系发布者重新上传。");
                string url=(string)asset["browser_download_url"];
                string expectedUrl="https://github.com/"+repo+"/releases/download/"+tag+"/"+name;
                if(!String.Equals(url,expectedUrl,StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("安装包地址与发布版本不一致。");
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
                VerifyInstaller(download,latest);
                status.Text="请按 Windows 安装程序的提示完成更新。";
                installing=true;close.Enabled=false;
                int result;
                using(var process=Process.Start(InstallerCommand(download))) {
                    await Task.Run(()=>process.WaitForExit());result=process.ExitCode;
                }
                installing=false;close.Enabled=true;
                if(result!=0 && result!=3010)throw new IOException("更新安装未完成（代码 "+result+"）。");
                string restart=exe;
                if(!File.Exists(restart) || ParseVersion(FileVersionInfo.GetVersionInfo(restart).FileVersion)<latest) {
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
            var exeAsset=new Dictionary<string,object>{{"name","fyto-Setup-1.0.0.39.exe"}};
            var msiAsset=new Dictionary<string,object>{{"name","fyto-Setup-1.0.0.39.msi"}};
            var version=new Version(1,0,0,39);
            if(FindInstaller(new[]{exeAsset},version)!=exeAsset ||
               FindInstaller(new[]{exeAsset,msiAsset},version)!=msiAsset ||
               FindInstaller(new[]{exeAsset,msiAsset},new Version(1,0,0,40))!=null)return 5;
            var command=InstallerCommand(@"C:\Test Directory\fyto-Setup-1.0.0.39.msi");
            if(Path.GetFileName(command.FileName)!="msiexec.exe" ||
               command.Arguments!="/i \"C:\\Test Directory\\fyto-Setup-1.0.0.39.msi\" /norestart")return 6;
            return 0;
        }
        if(args.Length==3 && args[0]=="--verify-msi") {
            try { VerifyInstaller(args[1],ParseVersion(args[2]));return 0; }
            catch { return 7; }
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
