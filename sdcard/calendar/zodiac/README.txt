QDTech Calendar Zodiac SD data

Copy the top-level calendar folder to the root of a FAT-formatted SD card.
The final device paths are:

  /calendar/zodiac/details.tsv
  /calendar/zodiac/images/aries.jpg
  /calendar/zodiac/images/taurus.jpg
  /calendar/zodiac/images/gemini.jpg
  /calendar/zodiac/images/cancer.jpg
  /calendar/zodiac/images/leo.jpg
  /calendar/zodiac/images/virgo.jpg
  /calendar/zodiac/images/libra.jpg
  /calendar/zodiac/images/scorpio.jpg
  /calendar/zodiac/images/sagittarius.jpg
  /calendar/zodiac/images/capricorn.jpg
  /calendar/zodiac/images/aquarius.jpg
  /calendar/zodiac/images/pisces.jpg

Image requirements:

  JPEG, RGB, square, recommended 160 x 160 pixels, maximum input 256 KiB.
  The firmware decodes only the selected sign into PSRAM and releases it when
  leaving the Zodiac page.

details.tsv is UTF-8 and uses four tab-separated fields:

  sign_id<TAB>page_index<TAB>page_title<TAB>page_text

page_index runs from 0 through 5. Use the two characters \n inside page_text
for an intentional line break. Literal tabs and literal newlines are not
allowed inside a field.
