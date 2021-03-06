﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime; // For IBuffer.ToArray()
using System.Text;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Storage.Streams;
using Windows.UI;
using Windows.UI.Xaml.Media.Imaging;

namespace ObjectTrackingDemo
{
    class ImageProcessingUtils
    {
        /// <summary>
        /// Converts RGB888 pixel to YUV pixel.
        /// 
        /// From https://msdn.microsoft.com/en-us/library/aa917087.aspx
        /// Y = ( (  66 * R + 129 * G +  25 * B + 128) >> 8) +  16
        /// U = ( ( -38 * R -  74 * G + 112 * B + 128) >> 8) + 128
        /// V = ( ( 112 * R -  94 * G -  18 * B + 128) >> 8) + 128
        /// </summary>
        /// <param name="r"></param>
        /// <param name="g"></param>
        /// <param name="b"></param>
        /// <returns></returns>
        public static byte[] RGBToYUV(byte r, byte g, byte b)
        {
            byte[] yuv = new byte[3];
            yuv[0] = (byte)(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
            yuv[1] = (byte)(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
            yuv[2] = (byte)(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
            return yuv;
        }

        /// <summary>
        /// Converts 8-bit YUV pixel to RGB888 pixel.
        /// </summary>
        /// <param name="y"></param>
        /// <param name="u"></param>
        /// <param name="v"></param>
        /// <returns></returns>
        public static byte[] YUVToRGB(byte y, byte u, byte v)
        {
            int c = y - 16;
            int d = u - 128;
            int e = v - 128;

            byte[] rgb = new byte[3];
            rgb[0] = (byte)Clip((298 * c + 409 * e + 128) >> 8);
            rgb[1] = (byte)Clip((298 * c - 100 * d - 208 * e + 128) >> 8);
            rgb[2] = (byte)Clip((298 * c + 516 * d + 128) >> 8);

            return rgb;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="yuvPixelArray"></param>
        /// <param name="pixelWidth"></param>
        /// <param name="pixelHeight"></param>
        /// <param name="ignoreAlpha"></param>
        /// <returns></returns>
        public static byte[] NV12PixelArrayToRGBPixelArray(byte[] yuvPixelArray, int pixelWidth, int pixelHeight, bool ignoreAlpha = false)
        {
            if (yuvPixelArray.Length < pixelWidth * pixelHeight * 1.5f)
            {
                System.Diagnostics.Debug.WriteLine("NV12PixelArrayToRGBPixelArray: Too few bytes: Was expecting "
                    + pixelWidth * pixelHeight * 1.5f + ", but got " + yuvPixelArray.Length);
                return null;
            }

            int bytesPerPixel = ignoreAlpha ? 3 : 4;
            int bytesPerLineRGB = pixelWidth * bytesPerPixel;
            byte[] rgbPixelArray = new byte[bytesPerLineRGB * pixelHeight];

            byte yy = 0;
            byte u = 0;
            byte v = 0;

            int uvPlaneStartIndex = pixelWidth * pixelHeight - 1;

            byte[] rgbPixel = null;
            int rgbIndex = 0;

            for (int y = 0; y < pixelHeight; ++y)
            {
                int cumulativeYIndex = y * pixelWidth;
                int cumulativeUVPlaneIndex = y / 2 * pixelWidth + uvPlaneStartIndex;

                for (int x = 0; (x + 1) < pixelWidth; x += 2)
                {
                    int yAsInt = ((int)yuvPixelArray[cumulativeYIndex + x] + (int)yuvPixelArray[cumulativeYIndex + x + 1]
                        + (int)yuvPixelArray[cumulativeYIndex + pixelWidth + x] + (int)yuvPixelArray[cumulativeYIndex + pixelWidth + x + 1]) / 4;
                    yy = (byte)yAsInt;
                    u = yuvPixelArray[cumulativeUVPlaneIndex + x];
                    v = yuvPixelArray[cumulativeUVPlaneIndex + x + 1];

                    rgbPixel = YUVToRGB(yy, u, v);

                    for (int i = 0; i < 2; ++i)
                    {
                        rgbPixelArray[rgbIndex] = rgbPixel[0]; // B
                        rgbPixelArray[rgbIndex + 1] = rgbPixel[1]; // G
                        rgbPixelArray[rgbIndex + 2] = rgbPixel[2]; // R

                        if (!ignoreAlpha)
                        {
                            rgbPixelArray[rgbIndex + 3] = 0xff;
                        }

                        rgbIndex += bytesPerPixel;
                    }
                }
            }

            return rgbPixelArray;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="yuvPixelArray"></param>
        /// <param name="pixelWidth"></param>
        /// <param name="pixelHeight"></param>
        /// <param name="ignoreAlpha"></param>
        /// <returns></returns>
        public static byte[] YUY2PixelArrayToRGBPixelArray(byte[] yuvPixelArray, int pixelWidth, int pixelHeight, bool ignoreAlpha = false)
        {
            if (yuvPixelArray.Length < pixelWidth * pixelHeight * 2)
            {
                System.Diagnostics.Debug.WriteLine("YUY2PixelArrayToRGBPixelArray: Too few bytes: Was expecting "
                                                   + pixelWidth * pixelHeight * 2 + ", but got " + yuvPixelArray.Length);

                return null;
            }

            int bytesPerPixel = ignoreAlpha ? 3 : 4;
            int bytesPerLineRGB = pixelWidth * bytesPerPixel;
            byte[] rgbPixelArray = new byte[bytesPerLineRGB * pixelHeight];

            byte yy = 0;
            byte u = 0;
            byte v = 0;

            byte[] rgbPixel = null;
            int rgbIndex = 0;

            for (int y = 0; y < pixelHeight; ++y)
            {
                for (int x = 0; (x + 1) < pixelWidth; x += 2)
                {
                    int index = (y * pixelWidth + x) << 1;
                    yy = (byte)((yuvPixelArray[index] + yuvPixelArray[index + 2]) >> 1);
                    u = yuvPixelArray[index + 3];
                    v = yuvPixelArray[index + 1];

                    rgbPixel = YUVToRGB(yy, u, v);

                    for (int i = 0; i < 2; ++i)
                    {
                        rgbPixelArray[rgbIndex] = rgbPixel[0]; // B
                        rgbPixelArray[rgbIndex + 1] = rgbPixel[1]; // G
                        rgbPixelArray[rgbIndex + 2] = rgbPixel[2]; // R

                        if (!ignoreAlpha)
                        {
                            rgbPixelArray[rgbIndex + 3] = 0xff;
                        }

                        rgbIndex += bytesPerPixel;
                    }
                }
            }

            return rgbPixelArray;
        }

        /// <summary>
        /// Resolves and returns the color at the given point in the given image.
        /// 
        /// Note: At the moment the implementation expects that there are four bytes per pixels in
        /// the given array.
        /// </summary>
        /// <param name="pixelArray">A byte array containing the image data.</param>
        /// <param name="width">The width of the image.</param>
        /// <param name="height">The height of the image.</param>
        /// <param name="point">The point of the desired color.</param>
        /// <returns>A Color instance.</returns>
        public static Color GetColorAtPoint(byte[] pixelArray, uint width, uint height, Point point)
        {
            int bytesPerPixel = (int)(pixelArray.Length / height / width);

            System.Diagnostics.Debug.WriteLine("Image size is " + width + "x" + height
                + ", byte array length is " + pixelArray.Length + " => number of bytes per pixel is " + bytesPerPixel);

            int index = (int)(point.Y * width * bytesPerPixel + point.X * bytesPerPixel);

            System.Diagnostics.Debug.WriteLine("Picked index is " + index + ": "
                + pixelArray[index] + " "
                + pixelArray[index + 1] + " "
                + pixelArray[index + 2] + " "
                + pixelArray[index + 3]);

            return new Color
            {
                A = 0xFF, //pixelArray[index],
                R = pixelArray[index + 2],
                G = pixelArray[index + 1],
                B = pixelArray[index]
            };
        }

        #region GetColorAtPoint overloads

        public static Color GetColorAtPoint(IBuffer pixelBuffer, uint width, uint height, Point point)
        {
            return GetColorAtPoint(pixelBuffer.ToArray(), width, height, point);
        }

        public static Color GetColorAtPoint(WriteableBitmap bitmap, uint width, uint height, Point point)
        {
            return GetColorAtPoint(bitmap.PixelBuffer.ToArray(), width, height, point);
        }

        #endregion

        public static double BrightnessFromColor(Color color)
        {
            return (color.R + color.G + color.B) / 3;
        }

        public static Color ApplyBrightnessToColor(Color color, double brightness)
        {
            if (brightness >= 0 && brightness <= 255 && brightness != BrightnessFromColor(color))
            {
                int change = (BrightnessFromColor(color) > brightness) ? -1 : 1;

                while (BrightnessFromColor(color) != brightness)
                {
                    if ((change == 1 && color.R < 255)
                        || (change == -1 && color.R > 0))
                    {
                        color.R += (byte)change;
                    }

                    if ((change == 1 && color.G < 255)
                        || (change == -1 && color.G > 0))
                    {
                        color.G += (byte)change;
                    }

                    if ((change == 1 && color.B < 255)
                        || (change == -1 && color.B > 0))
                    {
                        color.B += (byte)change;
                    }
                }
            }

            return color;
        }

        private static byte Clip(byte value, byte min = 0, byte max = 255)
        {
            if (value < min)
            {
                return min;
            }

            if (value > max)
            {
                return max;
            }

            return value;
        }

        private static int Clip(int value, int min = 0, int max = 255)
        {
            if (value < min)
            {
                return min;
            }

            if (value > max)
            {
                return max;
            }

            return value;
        }
    }
}
