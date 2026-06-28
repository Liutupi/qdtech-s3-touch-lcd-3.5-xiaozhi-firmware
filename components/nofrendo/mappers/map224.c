/*
** Mapper 224 / KT-008.  It is an MMC3-compatible board with extra
** outer bank registers in $5000-$5FFF, commonly found with bad iNES
** headers that claim mapper 4.
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

static uint8 expreg[7];
static uint8 bankreg[8];
static uint8 reg;
static uint8 command;
static uint16 vrombase;

static int map224_prg_bank(uint32 address, uint8 bank)
{
   int prg_mask_mmc3 = (expreg[3] & 0x10 ? 0x00 : 0x0F)
                     | (expreg[0] & 0x40 ? 0x00 : 0x10)
                     | (expreg[1] & 0x80 ? 0x00 : 0x20)
                     | (expreg[1] & 0x40 ? 0x40 : 0x00)
                     | (expreg[1] & 0x20 ? 0x80 : 0x00);
   int prg_mask_gnrom = expreg[3] & 0x10 ? (expreg[1] & 0x02 ? 0x03 : 0x01) : 0x00;
   int prg_offset = (expreg[3] & 0x000E)
                  | ((expreg[0] << 4) & 0x0070)
                  | ((expreg[1] << 3) & 0x0080)
                  | ((expreg[1] << 6) & 0x0300)
                  | ((expreg[0] << 6) & 0x0C00)
                  | ((~expreg[1] << 12) & 0x1000);

   prg_offset &= ~(prg_mask_mmc3 | prg_mask_gnrom);
   return (bank & prg_mask_mmc3) | prg_offset | ((address >> 13) & prg_mask_gnrom);
}

static int map224_chr_bank(uint32 address, uint8 bank)
{
   int chr_mask_mmc3 = expreg[3] & 0x10 ? 0x00 : (expreg[0] & 0x80 ? 0x7F : 0xFF);
   int chr_mask_gnrom = expreg[3] & 0x10 ? 0x07 : 0x00;
   int chr_offset = ((expreg[0] << 9) & 0x0C00)
                  | ((expreg[0] << 4) & 0x0380)
                  | ((expreg[2] << 3) & 0x0078);

   chr_offset &= ~(chr_mask_mmc3 | chr_mask_gnrom);
   return (bank & chr_mask_mmc3) | chr_offset | ((address >> 10) & chr_mask_gnrom);
}

static void map224_bankrom(uint32 address, uint8 bank)
{
   mmc_bankrom(8, address, map224_prg_bank(address, bank));
}

static void map224_bankvrom(uint32 address, uint8 bank)
{
   mmc_bankvrom(1, address, map224_chr_bank(address, bank));
}

static void map224_update_fixed_bank(void)
{
   if (command & 0x40)
      map224_bankrom(0x8000, (mmc_getinfo()->rom_banks * 2) - 2);
   else
      map224_bankrom(0xC000, (mmc_getinfo()->rom_banks * 2) - 2);
   map224_bankrom(0xE000, (mmc_getinfo()->rom_banks * 2) - 1);
}

static void map224_sync_banks(void)
{
   uint16 base = (command & 0x80) ? 0x1000 : 0x0000;

   map224_bankvrom(base ^ 0x0000, bankreg[0] & 0xFE);
   map224_bankvrom(base ^ 0x0400, (bankreg[0] & 0xFE) + 1);
   map224_bankvrom(base ^ 0x0800, bankreg[1] & 0xFE);
   map224_bankvrom(base ^ 0x0C00, (bankreg[1] & 0xFE) + 1);
   map224_bankvrom(base ^ 0x1000, bankreg[2]);
   map224_bankvrom(base ^ 0x1400, bankreg[3]);
   map224_bankvrom(base ^ 0x1800, bankreg[4]);
   map224_bankvrom(base ^ 0x1C00, bankreg[5]);

   map224_bankrom((command & 0x40) ? 0xC000 : 0x8000, bankreg[6]);
   map224_bankrom(0xA000, bankreg[7]);
   map224_update_fixed_bank();
}

static void map224_write_mmc3(uint32 address, uint8 value)
{
   switch (address & 0xE001)
   {
   case 0x8000:
      command = value;
      vrombase = (command & 0x80) ? 0x1000 : 0x0000;
      if (reg != (value & 0x40))
         map224_sync_banks();
      reg = value & 0x40;
      break;

   case 0x8001:
      bankreg[command & 0x07] = value;
      switch (command & 0x07)
      {
      case 0:
         value &= 0xFE;
         map224_bankvrom(vrombase ^ 0x0000, value);
         map224_bankvrom(vrombase ^ 0x0400, value + 1);
         break;
      case 1:
         value &= 0xFE;
         map224_bankvrom(vrombase ^ 0x0800, value);
         map224_bankvrom(vrombase ^ 0x0C00, value + 1);
         break;
      case 2:
         map224_bankvrom(vrombase ^ 0x1000, value);
         break;
      case 3:
         map224_bankvrom(vrombase ^ 0x1400, value);
         break;
      case 4:
         map224_bankvrom(vrombase ^ 0x1800, value);
         break;
      case 5:
         map224_bankvrom(vrombase ^ 0x1C00, value);
         break;
      case 6:
         map224_bankrom((command & 0x40) ? 0xC000 : 0x8000, value);
         break;
      case 7:
         map224_bankrom(0xA000, value);
         break;
      }
      break;

   case 0xA000:
      if (0 == (mmc_getinfo()->flags & ROM_FLAG_FOURSCREEN))
         ppu_mirror(value & 1 ? 0 : 0, value & 1 ? 0 : 1, value & 1 ? 1 : 0, 1);
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

static void map224_write_outer(uint32 address, uint8 value)
{
   int index = address & 7;
   if ((expreg[3] & 0x80) && index != 2)
      return;

   if (index == 2)
   {
      if (expreg[2] & 0x80)
         value = (value & 0x0F) | (expreg[2] & ~0x0F);
      value &= ((~(expreg[2] >> 3)) & 0x0E) | 0xF1;
   }

   if (index <= 5)
   {
      expreg[index] = value;
      map224_sync_banks();
   }
}

static void map224_write(uint32 address, uint8 value)
{
   if (address >= 0x5000 && address <= 0x5FFF)
      map224_write_outer(address, value);
   else
      map224_write_mmc3(address, value);
}

static void map224_hblank(int vblank)
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

static void map224_init(void)
{
   for (int i = 0; i < 7; ++i)
      expreg[i] = 0;
   for (int i = 0; i < 8; ++i)
      bankreg[i] = 0;
   irq.counter = irq.latch = 0;
   irq.enabled = irq.reset = false;
   reg = command = 0;
   vrombase = 0x0000;
   map224_sync_banks();
}

static map_memwrite map224_memwrite[] =
{
   { 0x5000, 0x5FFF, map224_write },
   { 0x8000, 0xFFFF, map224_write },
   {     -1,     -1, NULL }
};

mapintf_t map224_intf =
{
   224,
   "KT-008",
   map224_init,
   NULL,
   map224_hblank,
   NULL,
   NULL,
   NULL,
   map224_memwrite,
   NULL
};
