#include "lr20xx_radio_bluetooth_le_types_str.h"

const char *lr20xx_radio_bluetooth_le_phy_to_str(const lr20xx_radio_bluetooth_le_phy_t value)
{
  switch (value)
  {
    case LR20XX_RADIO_BLUETOOTH_LE_PHY_LE_1M:
    {
      return (const char *) "1M";
    }

    case LR20XX_RADIO_BLUETOOTH_LE_PHY_LE_2M:
    {
      return (const char *) "2M";
    }

    case LR20XX_RADIO_BLUETOOTH_LE_PHY_LE_CODED_500KB:
    {
      return (const char *) "CODED_500KB";
    }

    case LR20XX_RADIO_BLUETOOTH_LE_PHY_LE_CODED_125KB:
    {
      return (const char *) "CODED_125KB";
    }

    default:
    {
      return (const char *) "Unknown";
    }

  }

}

const char *lr20xx_radio_bluetooth_le_phy_channel_pdu_to_str(const lr20xx_radio_bluetooth_le_phy_channel_pdu_t value)
{
  switch (value)
  {
    case LR20XX_RADIO_BLUETOOTH_LE_PHY_CHANNEL_PDU_ADVERTISING:
    {
      return (const char *) "ADVERTISING";
    }

    case LR20XX_RADIO_BLUETOOTH_LE_PHY_CHANNEL_PDU_DATA_16_BIT_HEADER:
    {
      return (const char *) "DATA_16_BIT_HEADER";
    }

    case LR20XX_RADIO_BLUETOOTH_LE_PHY_CHANNEL_PDU_DATA_24_BIT_HEADER:
    {
      return (const char *) "DATA_24_BIT_HEADER";
    }

    default:
    {
      return (const char *) "Unknown";
    }

  }

}

