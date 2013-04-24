using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using ArdupilotMega;
using System.IO.Ports;
using System.IO;
using System.Linq;

namespace IOBoard
{
    public partial class MainForm : Form
    {
        /// <summary>
        /// 328 eeprom memory
        /// </summary>
        byte[] eeprom = new byte[1024];

        ushort[] patterns = new ushort[32];

        SerialPort comPort = new SerialPort();

        public MainForm()
        {
            InitializeComponent();

            // default patterns
            patterns[0] = 0xF000;
            patterns[1] = 0xf0f0;
            patterns[2] = 0xf804;
            patterns[3] = 0xcccc;
            patterns[4] = 0x8888;
            patterns[5] = 0xaaa0;
            patterns[6] = 0xaaaa;
            patterns[7] = 0;
            patterns[8] = 0;
            patterns[9] = 0;
            patterns[10] = 0;
            patterns[11] = 0;
            patterns[12] = 0;
            patterns[13] = 0;
            patterns[14] = 0;
            patterns[15] = 0;
        }

        private void CMB_ComPort_Click(object sender, EventArgs e)
        {
            CMB_ComPort.Items.Clear();
            CMB_ComPort.Items.AddRange(GetPortNames());
        }

        private void BUT_ReadIOB_Click(object sender, EventArgs e)
        {
            toolStripProgressBar1.Style = ProgressBarStyle.Continuous;
            this.toolStripStatusLabel1.Text = "";

            bool fail = false;
            ArduinoSTK sp;

            try
            {
                if (comPort.IsOpen)
                    comPort.Close();

                sp = new ArduinoSTK();
                sp.PortName = CMB_ComPort.Text;
                sp.BaudRate = 57600;
                sp.DtrEnable = true;

                sp.Open();
            }
            catch { MessageBox.Show("Error opening com port", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error); return; }

            if (sp.connectAP())
            {
                try
                {
                    eeprom = sp.download(1024);
                }
                catch (Exception ex)
                {
                    fail = true;
                    MessageBox.Show(ex.Message);
                }
            }
            else
            {
                MessageBox.Show("Failed to talk to bootloader");
                fail = true;
            }

            sp.Close();

            int startmbind = 64;

            if (!fail)
            {
                for (int pos = 0; pos < 16 ;pos++) {
                    patterns[pos] = (ushort)((ushort)(eeprom[pos*2] << 8) + eeprom[pos*2 + 1]);
                    
                    Control[] ctls = this.Controls.Find("pattern" + (pos+1), true);
                    if (ctls.Length > 0)
                    {
                        ((Pattern)ctls[0]).Value = patterns[pos];
                        ((Pattern)ctls[0]).FlightMode = (ushort)(eeprom[startmbind + pos * 2]);
                    }
                }
            }

            printeeprom();

                if (!fail)
                    MessageBox.Show("Done!");
        }
        void printeeprom()
        {
            for (int a = 0; a < 200; a++)
            {
                while (a % 15 != 0 || a == 0)
                {
                    Console.Write("{0,02:X}", eeprom[a]);

                    a++;
                }
                Console.WriteLine();
            }
        }

        private void BUT_WriteIOB_Click(object sender, EventArgs e)
        {
            toolStripProgressBar1.Style = ProgressBarStyle.Continuous;
            this.toolStripStatusLabel1.Text = "";

            foreach (Control ctl in tabPage2.Controls) 
            {
                if (ctl.GetType() == typeof(Pattern))
                {
                    Pattern pat = ctl as Pattern;

                    int no = 0;

                    if (int.TryParse(pat.Name.Substring(pat.Name.Length - 2), out no))
                    {
                        eeprom[(no - 1) * 2] = (byte)(pat.Value >> 8);
                        eeprom[(no - 1) * 2 + 1] = (byte)(pat.Value & 0xff);

                        eeprom[64 + (no - 1) * 2] = (byte)pat.FlightMode;
                        eeprom[64 + (no - 1) * 2 + 1] = (byte)pat.FlightMode;
                    }
                    else if (int.TryParse(pat.Name.Substring(pat.Name.Length - 1), out no))
                    {
                        eeprom[(no - 1) * 2] = (byte)(pat.Value >> 8);
                        eeprom[(no - 1) * 2 + 1] = (byte)(pat.Value & 0xff);

                        eeprom[64 + (no - 1) * 2] = (byte)pat.FlightMode;
                        eeprom[64 + (no - 1) * 2 + 1] = (byte)pat.FlightMode;
                    }
                }
            }

            printeeprom();

            ArduinoSTK sp;

            try
            {
                if (comPort.IsOpen)
                    comPort.Close();

                sp = new ArduinoSTK();
                sp.PortName = CMB_ComPort.Text;
                sp.BaudRate = 57600;
                sp.DtrEnable = true;

                sp.Open();
            }
            catch { MessageBox.Show("Error opening com port", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error); return; }

            if (sp.connectAP())
            {
                try
                {
                    if (sp.upload(eeprom, 0, 126, 0))
                    {
                        MessageBox.Show("Done!");
                    }
                    else
                    {
                        MessageBox.Show("Failed to upload new settings");
                    }
                }
                catch (Exception ex)
                {
                    MessageBox.Show(ex.Message);
                }
            }
            else
            {
                MessageBox.Show("Failed to talk to bootloader");
            }

            sp.Close();
        }

        private string[] GetPortNames()
        {
            string[] devs = new string[0];

            if (Directory.Exists("/dev/"))
                devs = Directory.GetFiles("/dev/", "*ACM*");

            string[] ports = SerialPort.GetPortNames();

            string[] all = new string[devs.Length + ports.Length];

            devs.CopyTo(all, 0);
            ports.CopyTo(all, devs.Length);

            return all;
        }

        private void helpToolStripMenuItem_Click(object sender, EventArgs e)
        {
            System.Diagnostics.Process.Start("http://www.jdrones.com/jDoc/wiki:s_ioboardconfig");
        }

        private void saveIOBoardFileToolStripMenuItem_Click(object sender, EventArgs e)
        {
            SaveFileDialog sfd = new SaveFileDialog() { Filter = "*.iob|*.iob" };

            sfd.ShowDialog();

            if (sfd.FileName != "")
            {
                try
                {
                    using (StreamWriter sw = new StreamWriter(sfd.OpenFile()))
                    {
                        List<Pattern> lst = this.Controls.OfType<Pattern>().ToList();

                        foreach (Pattern item in lst)
                        {
                            if (item != null)
                                sw.WriteLine("{0}\t{1}\t{2}", item.Name, item.Value, item.FlightMode);
                        }
                        sw.Close();
                    }
                }
                catch
                {
                    MessageBox.Show("Error writing file");
                }
            }
        }

        private void loadIOBoardFileToolStripMenuItem_Click(object sender, EventArgs e)
        {
            OpenFileDialog ofd = new OpenFileDialog() { Filter = "*.iob|*.iob" };

            ofd.ShowDialog();

            if (ofd.FileName != "")
            {
                try
                {
                    using (StreamReader sr = new StreamReader(ofd.OpenFile()))
                    {
                        while (!sr.EndOfStream)
                        {
                            string[] strings = sr.ReadLine().Split(new char[] { '\t' }, StringSplitOptions.RemoveEmptyEntries);

                            Pattern pat = (Pattern)this.Controls.OfType<Pattern>().Single(f => (f.Name == strings[0]));

                            if (pat != null)
                            {
                                pat.Value = ushort.Parse(strings[1]);
                                pat.FlightMode = ushort.Parse(strings[2]);
                            }
                        }
                    }
                }
                catch
                {
                    MessageBox.Show("Error Reading file");
                }
            }

        }

        private void updateFirmwareToolStripMenuItem_Click(object sender, EventArgs e)
        {
            toolStripProgressBar1.Style = ProgressBarStyle.Continuous;
            this.toolStripStatusLabel1.Text = "";

            OpenFileDialog ofd = new OpenFileDialog();
            ofd.Filter = "*.hex|*.hex";

            ofd.ShowDialog();

            if (ofd.FileName != "")
            {
                byte[] FLASH;
                try
                {
                    toolStripStatusLabel1.Text = "Reading Hex File";

                    statusStrip1.Refresh();

                    FLASH = readIntelHEXv2(new StreamReader(ofd.FileName));
                }
                catch { MessageBox.Show("Bad Hex File"); return; }

                bool fail = false;
                ArduinoSTK sp;

                try
                {
                    if (comPort.IsOpen)
                        comPort.Close();

                    sp = new ArduinoSTK();
                    sp.PortName = CMB_ComPort.Text;
                    sp.BaudRate = 57600;
                    sp.DtrEnable = true;

                    sp.Open();
                }
                catch { MessageBox.Show("Error opening com port", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error); return; }

                toolStripStatusLabel1.Text = "Connecting to Board";

                if (sp.connectAP())
                {
                    sp.Progress += new ArduinoSTK.ProgressEventHandler(sp_Progress);
                    try
                    {
                        if (!sp.uploadflash(FLASH, 0, FLASH.Length, 0))
                        {
                            if (sp.IsOpen)
                                sp.Close();

                            MessageBox.Show("Upload failed. Lost sync. Try using Arduino to upload instead",
                                "Error",
                                MessageBoxButtons.OK,
                                MessageBoxIcon.Warning);
                        }
                    }
                    catch (Exception ex)
                    {
                        fail = true;
                        MessageBox.Show(ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }

                }
                else
                {
                    MessageBox.Show("Failed to talk to bootloader");
                }

                sp.Close();

                if (!fail)
                {

                    toolStripStatusLabel1.Text = "Done";

                    MessageBox.Show("Done!");
                }
                else
                {
                    toolStripStatusLabel1.Text = "Failed";
                }
            }
        }

        byte[] readIntelHEXv2(StreamReader sr)
        {
            byte[] FLASH = new byte[1024 * 1024];

            int optionoffset = 0;
            int total = 0;
            bool hitend = false;

            while (!sr.EndOfStream)
            {
                toolStripProgressBar1.Value = (int)(((float)sr.BaseStream.Position / (float)sr.BaseStream.Length) * 100);

                string line = sr.ReadLine();

                if (line.StartsWith(":"))
                {
                    int length = Convert.ToInt32(line.Substring(1, 2), 16);
                    int address = Convert.ToInt32(line.Substring(3, 4), 16);
                    int option = Convert.ToInt32(line.Substring(7, 2), 16);
                    Console.WriteLine("len {0} add {1} opt {2}", length, address, option);

                    if (option == 0)
                    {
                        string data = line.Substring(9, length * 2);
                        for (int i = 0; i < length; i++)
                        {
                            byte byte1 = Convert.ToByte(data.Substring(i * 2, 2), 16);
                            FLASH[optionoffset + address] = byte1;
                            address++;
                            if ((optionoffset + address) > total)
                                total = optionoffset + address;
                        }
                    }
                    else if (option == 2)
                    {
                        optionoffset = (int)Convert.ToUInt16(line.Substring(9, 4), 16) << 4;
                    }
                    else if (option == 1)
                    {
                        hitend = true;
                    }
                    int checksum = Convert.ToInt32(line.Substring(line.Length - 2, 2), 16);

                    byte checksumact = 0;
                    for (int z = 0; z < ((line.Length - 1 - 2) / 2); z++) // minus 1 for : then mins 2 for checksum itself
                    {
                        checksumact += Convert.ToByte(line.Substring(z * 2 + 1, 2), 16);
                    }
                    checksumact = (byte)(0x100 - checksumact);

                    if (checksumact != checksum)
                    {
                        MessageBox.Show("The hex file loaded is invalid, please try again.");
                        throw new Exception("Checksum Failed - Invalid Hex");
                    }
                }
                //Regex regex = new Regex(@"^:(..)(....)(..)(.*)(..)$"); // length - address - option - data - checksum
            }

            if (!hitend)
            {
                MessageBox.Show("The hex file did no contain an end flag. aborting");
                throw new Exception("No end flag in file");
            }

            Array.Resize<byte>(ref FLASH, total);

            return FLASH;
        }

        void sp_Progress(int progress)
        {
            toolStripStatusLabel1.Text = "Uploading " + progress + " %";
            toolStripProgressBar1.Value = progress;

            statusStrip1.Refresh();
        }

        private void resetBoardSettingsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            ArduinoSTK sp;

            try
            {
                if (comPort.IsOpen)
                    comPort.Close();

                sp = new ArduinoSTK();
                sp.PortName = CMB_ComPort.Text;
                sp.BaudRate = 57600;
                sp.DtrEnable = true;

                sp.Open();
            }
            catch { MessageBox.Show("Error opening com port", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error); return; }

            eeprom = new byte[eeprom.Length];

            if (sp.connectAP())
            {
                try
                {
                    if (sp.upload(eeprom, 0, 1024, 0))
                    {
                        MessageBox.Show("Done!");
                    }
                    else
                    {
                        MessageBox.Show("Failed to upload new settings");
                    }
                }
                catch (Exception ex)
                {
                    MessageBox.Show(ex.Message);
                }
            }
            else
            {
                MessageBox.Show("Failed to talk to bootloader");
            }

            sp.Close();
        }

        private void exitToolStripMenuItem_Click(object sender, EventArgs e)
        {
            Application.Exit();
        }

        public string PublishVersion
        {
            get
            {
                if (System.Deployment.Application.ApplicationDeployment.IsNetworkDeployed)
                {
                    Version ver = System.Deployment.Application.ApplicationDeployment.CurrentDeployment.CurrentVersion;
                    return string.Format("{0}.{1}.{2}.{3}", ver.Major, ver.Minor, ver.Build, ver.Revision);
                }
                else
                    return "Not Published";
            }
        }

        public string AssemblyVersion
        {
            get
            {
                return System.Reflection.Assembly.GetExecutingAssembly().GetName().Version.ToString();
            }
        }

        private void MainForm_Load(object sender, EventArgs e)
        {
            String strMajor = System.Reflection.Assembly.GetEntryAssembly().GetName().Version.Major.ToString();
            String strMajRev = System.Reflection.Assembly.GetEntryAssembly().GetName().Version.MajorRevision.ToString();
            String strMinor = System.Reflection.Assembly.GetEntryAssembly().GetName().Version.Minor.ToString();
            String strMinRev = System.Reflection.Assembly.GetExecutingAssembly().GetName().Version.MinorRevision.ToString();
            LBL_Version.Text = strMajor + "." + strMajRev + "." + strMinor + "." + strMinRev;
            //LBL_Version.Text = AssemblyVersion;
        }

        private void label1_Click(object sender, EventArgs e)
        {

        }

        private void label3_Click(object sender, EventArgs e)
        {

        }

        private void label4_Click(object sender, EventArgs e)
        {

        }

        private void label5_Click(object sender, EventArgs e)
        {

        }

        private void pattern1_Load(object sender, EventArgs e)
        {

        }

        private void pattern2_Load(object sender, EventArgs e)
        {

        }

        private void pattern3_Load(object sender, EventArgs e)
        {

        }

        private void pattern4_Load(object sender, EventArgs e)
        {

        }

        private void pattern5_Load(object sender, EventArgs e)
        {

        }

        private void pattern6_Load(object sender, EventArgs e)
        {

        }

        private void pattern7_Load(object sender, EventArgs e)
        {

        }

        private void pattern8_Load(object sender, EventArgs e)
        {

        }

        private void menuStrip1_ItemClicked(object sender, ToolStripItemClickedEventArgs e)
        {

        }

        private void aboutToolStripMenuItem_Click(object sender, EventArgs e)
        {

            MessageBox.Show(string.Format("Jani Hirvinen            - General functionalities and UI{0}Michael Oborne        - Arduino libraries and original code{0}{0}Checkout more from http://www.jdrones.com/jDoc", Environment.NewLine), "Program Credits", MessageBoxButtons.OK, MessageBoxIcon.Information);
            //            box.ShowDialog();

        }

        private void label16_Click(object sender, EventArgs e)
        {

        }

        private void tabPage2_Click(object sender, EventArgs e)
        {

        }

        private void pattern3_Load_1(object sender, EventArgs e)
        {

        }

    }
}
