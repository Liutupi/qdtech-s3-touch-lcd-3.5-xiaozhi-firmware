/*
** Mapper 198 / TW MMC3+VRAM Rev. E.
** Used by several Tenchi wo Kurau II / Tun Shi Tian Di II dumps that are
** often misheadered as mapper 4.
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes.h>
#include <libsnss.h>

static struct
{
   int counter, latch;
   bool enabled, reset;
} irq;

static uint8 reg;
static uint8 command;
static uint16 vrombase;

static void map198_bankrom(uint32 address, uint8 value)
{
   if (value >= 0x50)
      value &= 0x4F;
   mmc_bankrom(8, address, value);
}

static uint8 map198_read_wram(uint32 address)
{
   rominfo_t *info = mmc_getinfo();
   if (info && info->sram)
      return info->sram[0x2000 + (address & 0x0FFF)];
   return 0xFF;
}

static void map198_write_wram(uint32 address, uint8 value)
{
   rominfo_t *info = mmc_getinfo();
   if (info && info->sram)
      info->sram[0x2000 + (address & 0x0FFF)] = value;
}

static void map198_write(uint32 address, uint8 value)
{
   switch (address & 0xE001)
   {
   case 0x8000:
      command = value;
      vrombase = (command & 0x80) ? 0x1000 : 0x0000;

      if (reg != (value & 0x40))
      {
         if (value & 0x40)
            map198_bankrom(0x8000, (mmc_getinfo()->rom_banks * 2) - 2);
         else
            map198_bankrom(0xC000, (mmc_getinfo()->rom_banks * 2) - 2);
      }
      reg = value & 0x40;
      break;

   case 0x8001:
      switch (command & 0x07)
      {
      case 0:
         value &= 0xFE;
         mmc_bankvrom(1, vrombase ^ 0x0000, value);
         mmc_bankvrom(1, vrombase ^ 0x0400, value + 1);
         break;
      case 1:
         value &= 0xFE;
         mmc_bankvrom(1, vrombase ^ 0x0800, value);
         mmc_bankvrom(1, vrombase ^ 0x0C00, value + 1);
         break;
      case 2:
         mmc_bankvrom(1, vrombase ^ 0x1000, value);
         break;
      case 3:
         mmc_bankvrom(1, vrombase ^ 0x1400, value);
         break;
      case 4:
         mmc_bankvrom(1, vrombase ^ 0x1800, value);
         break;
      case 5:
         mmc_bankvrom(1, vrombase ^ 0x1C00, value);
         break;
      case 6:
         map198_bankrom((command & 0x40) ? 0xC000 : 0x8000, value);
         break;
      case 7:
         map198_bankrom(0xA000, value);
         break;
      }
      break;

   case 0xA000:
      if (0 == (mmc_getinfo()->flags & ROM_FLAG_FOURSCREEN))
      {
         if (value & 1)
            ppu_mirror(0, 0, 1, 1);
         else
            ppu_mirror(0, 1, 0, 1);
      }
      break;

   case 0xA001:
      break;
   case 0xC000:
      irq.latch = value;
      break;
   case 0xC001:
      irq.reset = true;
      irq.counter = irq.latch;
      break;
   case 0xE000:
      irq.enabled = false;
      break;
   case 0xE001:
      irq.enabled = true;
      break;
   }

   if (irq.reset)
      irq.counter = irq.latch;
}

static void map198_hblank(int vblank)
{
   if (vblank)
      return;

   if (ppu_enabled())
   {
      if (irq.counter >= 0)
      {
         irq.reset = false;
         irq.counter--;
         if (irq.counter < 0 && irq.enabled)
         {
            irq.reset = true;
            nes_irq();
         }
      }
   }
}

static void map198_init(void)
{
   irq.counter = irq.latch = 0;
   irq.enabled = irq.reset = false;
   reg = command = 0;
   vrombase = 0x0000;
}

static map_memread map198_memread[] =
{
   { 0x5000, 0x5FFF, map198_read_wram },
   {     -1,     -1, NULL }
};

static map_memwrite map198_memwrite[] =
{
   { 0x5000, 0x5FFF, map198_write_wram },
   { 0x8000, 0xFFFF, map198_write },
   {     -1,     -1, NULL }
};

mapintf_t map198_intf =
{
   198,
   "TW MMC3+VRAM",
   map198_init,
   NULL,
   map198_hblank,
   NULL,
   NULL,
   map198_memread,
   map198_memwrite,
   NULL
};
