#include <noftypes.h>
#include <nes_mmc.h>
#include <nes.h>

static uint8 reg[16];
static uint8 scratch[4];
static uint8 flags;
static uint8 pad;

static void map83_sync(void)
{
   const int prg_and = 0x1F;

   switch (reg[1] & 0x18)
   {
   case 0x00:
      mmc_bankrom(16, 0x8000, reg[0]);
      mmc_bankrom(16, 0xC000, reg[0] | (prg_and >> 1));
      break;
   case 0x08:
      mmc_bankrom(16, 0x8000, reg[0]);
      mmc_bankrom(16, 0xC000, reg[0]);
      break;
   default:
      mmc_bankrom(8, 0x8000, ((reg[0] << 1) & ~prg_and) | (reg[4] & prg_and));
      mmc_bankrom(8, 0xA000, ((reg[0] << 1) & ~prg_and) | (reg[5] & prg_and));
      mmc_bankrom(8, 0xC000, ((reg[0] << 1) & ~prg_and) | (reg[6] & prg_and));
      mmc_bankrom(8, 0xE000, ((reg[0] << 1) & ~prg_and) | (0xFF & prg_and));
      break;
   }

   mmc_bankrom(8, 0x6000, reg[7]);

   if (mmc_getinfo()->vrom_banks >= 16)
   {
      mmc_bankvrom(2, 0x0000, reg[8]);
      mmc_bankvrom(2, 0x0800, reg[9]);
      mmc_bankvrom(2, 0x1000, reg[14]);
      mmc_bankvrom(2, 0x1800, reg[15]);
   }
   else
   {
      for (int i = 0; i < 8; ++i)
         mmc_bankvrom(1, i << 10, reg[8 | i]);
   }

   switch (reg[1] & 0x03)
   {
   case 0:
      ppu_mirror(0, 1, 0, 1);
      break;
   case 1:
      ppu_mirror(0, 0, 1, 1);
      break;
   case 2:
      ppu_mirror(0, 0, 0, 0);
      break;
   case 3:
      ppu_mirror(1, 1, 1, 1);
      break;
   }
}

static void map83_write(uint32 address, uint8 value)
{
   switch (address & 0x0318)
   {
   case 0x000:
   case 0x008:
   case 0x010:
   case 0x018:
   case 0x100:
   case 0x108:
   case 0x110:
   case 0x118:
      reg[(address >> 8) & 1] = value;
      map83_sync();
      break;

   case 0x200:
   case 0x208:
   case 0x210:
   case 0x218:
      reg[2 | (address & 1)] = value;
      if (address & 1)
         flags = (flags & ~0x80) | (reg[1] & 0x80);
      break;

   case 0x300:
   case 0x308:
      reg[4 | (address & 3)] = value;
      map83_sync();
      break;

   case 0x310:
      reg[8 | (address & 7)] = value;
      map83_sync();
      break;

   case 0x318:
      flags = (flags & ~0x40) | (value & 0x40);
      break;

   default:
      break;
   }
}

static uint8 map83_read_scratch(uint32 address)
{
   if (address & 0x100)
      return scratch[address & 3];
   return pad;
}

static void map83_write_scratch(uint32 address, uint8 value)
{
   scratch[address & 3] = value;
}

static void map83_clock_counter(void)
{
   uint16 counter = reg[2] | (reg[3] << 8);
   if ((flags & 0x80) && counter)
   {
      if (reg[1] & 0x40)
         --counter;
      else
         ++counter;
      if (!counter)
      {
         nes_irq();
         flags &= ~0x80;
      }
   }
   reg[2] = counter & 0xFF;
   reg[3] = (counter >> 8) & 0xFF;
}

static void map83_hblank(int vblank)
{
   if (vblank)
      return;

   int clocks = (flags & 0x40) ? 8 : 113;
   for (int i = 0; i < clocks; ++i)
      map83_clock_counter();
}

static void map83_init(void)
{
   for (int i = 0; i < 16; ++i)
      reg[i] = 0;
   for (int i = 0; i < 4; ++i)
      scratch[i] = 0;
   flags = 0;
   ++pad;
   map83_sync();
}

static map_memread map83_memread[] =
{
   { 0x5000, 0x5FFF, map83_read_scratch },
   {     -1,     -1, NULL }
};

static map_memwrite map83_memwrite[] =
{
   { 0x5000, 0x5FFF, map83_write_scratch },
   { 0x8000, 0xFFFF, map83_write },
   {     -1,     -1, NULL }
};

mapintf_t map83_intf =
{
   83,
   "Cony/Yoko",
   map83_init,
   NULL,
   map83_hblank,
   NULL,
   NULL,
   map83_memread,
   map83_memwrite,
   NULL
};
