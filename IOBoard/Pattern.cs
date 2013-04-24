using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Text;
using System.Windows.Forms;

namespace IOBoard
{
    public partial class Pattern : UserControl
    {
        enum Modes
        {
            STAB = 0,
            ACRO = 1,
            ALTH = 2,
            AUTO = 3,
            LOIT = 4,
            GUID = 5,
            RETL = 6,
            CIRC = 7,
            POSI = 8,
            LAND = 9,
            OFLO = 10,
            MANU = 11,
            FBWA = 12,
            FBWB = 13
        }

        int step = 1;

        /// <summary>
        /// returns bitmask of answer
        /// </summary>
        public ushort Value { get { return getStatus(); } set { setStatus(value); } }
        public ushort FlightMode { get { return (ushort)(int)Enum.Parse(typeof(Modes), CMB_flightmode.Text); } set { string ans = Enum.Parse(typeof(Modes), value.ToString()).ToString(); CMB_flightmode.Text = ans; } }

        ushort getStatus()
        {
            ushort answer = 0;
            // checkboxs are 1-16
            for (int a = 1; a <= 16; a++)
            {
                Control[] ctls = this.Controls.Find("checkBox" + a, true);

                if (ctls.Length > 0)
                {
                    CheckBox chkbox = ctls[0] as CheckBox;

                    answer += chkbox.Checked == true ? (ushort)(1 << (16 - a)) : (ushort)0;
                }
            }
            return answer;
        }

        void setStatus(ushort setas)
        {
            Console.WriteLine(this.Name + " " + setas.ToString("X"));
            for (int a = 1; a <= 16; a++)
            {
                Control[] ctls = this.Controls.Find("checkBox" + a, true);

                if (ctls.Length > 0)
                {
                    CheckBox chkbox = ctls[0] as CheckBox;

                    ushort and = (ushort)(setas & (1 << 16-a));

                    chkbox.Checked = (setas & (1 << 16 - a)) != 0;
                }
            }
        }

        public Pattern()
        {
            InitializeComponent();

            CMB_flightmode.DataSource = Enum.GetNames(typeof(Modes));
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            Bitmap image = new Bitmap(pictureBox1.Size.Width,pictureBox1.Size.Height);

            Graphics g = Graphics.FromImage(image);

            Control[] ctls = this.Controls.Find("checkBox" + step, true);

            if (ctls.Length > 0)
            {
                CheckBox chkbox = ctls[0] as CheckBox;
                if (chkbox.Checked)
                {
                    g.FillRectangle(Brushes.Red, 0, 0, image.Width, image.Height);
                }
                else
                {
                    g.FillRectangle(Brushes.LightGray, 0, 0, image.Width, image.Height);
                }
            }

            pictureBox1.Image = image;

            step++;

            if (step >= 17)
                step = 1;
        }

        private void CHK_demo_CheckedChanged(object sender, EventArgs e)
        {
            Bitmap image = new Bitmap(pictureBox1.Size.Width, pictureBox1.Size.Height);

            Graphics g = Graphics.FromImage(image);

            if (CHK_demo.Checked)
            {
                timer1.Start();
            }
            else
            {
                g.FillRectangle(Brushes.LightGray, 0, 0, image.Width, image.Height);
                timer1.Stop();
                
            }
        }

        private void Pattern_Load(object sender, EventArgs e)
        {

        }
    }
}
