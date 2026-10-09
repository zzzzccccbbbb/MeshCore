#include "lr20xx_radio_wi_sun_types_str.h"

const char *lr20xx_radio_wi_sun_operating_mode_to_str(const lr20xx_radio_wi_sun_operating_mode_t value)
{
  switch (value)
  {
    case LR20XX_RADIO_WI_SUN_OPERATING_MODE_1A:
    {
      return (const char *) "1A";
    }

    case LR20XX_RADIO_WI_SUN_OPERATING_MODE_1B:
    {
      return (const char *) "1B";
    }

    case LR20XX_RADIO_WI_SUN_OPERATING_MODE_2A:
    {
      return (const char *) "2A";
    }

    case LR20XX_RADIO_WI_SUN_OPERATING_MODE_2B:
    {
      return (const char *) "2B";
    }

    case LR20XX_RADIO_WI_SUN_OPERATING_MODE_3:
    {
      return (const char *) "3";
    }

    case LR20XX_RADIO_WI_SUN_OPERATING_MODE_4A:
    {
      return (const char *) "4A";
    }

    case LR20XX_RADIO_WI_SUN_OPERATING_MODE_4B:
    {
      return (const char *) "4B";
    }

    case LR20XX_RADIO_WI_SUN_OPERATING_MODE_5:
    {
      return (const char *) "5";
    }

    default:
    {
      return (const char *) "Unknown";
    }

  }

}

const char *lr20xx_radio_wi_sun_fcs_to_str(const lr20xx_radio_wi_sun_fcs_t value)
{
  switch (value)
  {
    case LR20XX_RADIO_WI_SUN_FCS_32_BIT_CRC:
    {
      return (const char *) "32_BIT_CRC";
    }

    case LR20XX_RADIO_WI_SUN_FCS_16_BIT_CRC:
    {
      return (const char *) "16_BIT_CRC";
    }

    default:
    {
      return (const char *) "Unknown";
    }

  }

}

const char *lr20xx_radio_wi_sun_whitening_to_str(const lr20xx_radio_wi_sun_whitening_t value)
{
  switch (value)
  {
    case LR20XX_RADIO_WI_SUN_WHITENING_DISABLED:
    {
      return (const char *) "DISABLED";
    }

    case LR20XX_RADIO_WI_SUN_WHITENING_ENABLED:
    {
      return (const char *) "ENABLED";
    }

    default:
    {
      return (const char *) "Unknown";
    }

  }

}

const char *lr20xx_radio_wi_sun_crc_source_to_str(const lr20xx_radio_wi_sun_crc_source_t value)
{
  switch (value)
  {
    case LR20XX_RADIO_WI_SUN_CRC_SOURCE_PAYLOAD:
    {
      return (const char *) "PAYLOAD";
    }

    case LR20XX_RADIO_WI_SUN_CRC_SOURCE_COMPUTED:
    {
      return (const char *) "COMPUTED";
    }

    default:
    {
      return (const char *) "Unknown";
    }

  }

}

const char *lr20xx_radio_wi_sun_payload_mode_to_str(const lr20xx_radio_wi_sun_payload_mode_t value)
{
  switch (value)
  {
    case LR20XX_RADIO_WI_SUN_PAYLOAD_MODE_NORMAL:
    {
      return (const char *) "NORMAL";
    }

    case LR20XX_RADIO_WI_SUN_PAYLOAD_MODE_SWITCH:
    {
      return (const char *) "SWITCH";
    }

    default:
    {
      return (const char *) "Unknown";
    }

  }

}

const char *lr20xx_radio_wi_sun_fec_mode_to_str(const lr20xx_radio_wi_sun_fec_mode_t value)
{
  switch (value)
  {
    case LR20XX_RADIO_WI_SUN_FEC_MODE_NONE:
    {
      return (const char *) "NONE";
    }

    case LR20XX_RADIO_WI_SUN_FEC_MODE_NRNSC_WITH_INTERLEAVER:
    {
      return (const char *) "NRNSC_WITH_INTERLEAVER";
    }

    case LR20XX_RADIO_WI_SUN_FEC_MODE_RSC_WITHOUT_INTERLEAVER:
    {
      return (const char *) "RSC_WITHOUT_INTERLEAVER";
    }

    case LR20XX_RADIO_WI_SUN_FEC_MODE_RSC_WITH_INTERLEAVER:
    {
      return (const char *) "RSC_WITH_INTERLEAVER";
    }

    default:
    {
      return (const char *) "Unknown";
    }

  }

}

