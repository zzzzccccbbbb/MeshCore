#include "lr20xx_rttof_types_str.h"

const char *lr20xx_rttof_mode_to_str(const lr20xx_rttof_mode_t value)
{
  switch (value)
  {
    case LR20XX_RTTOF_MODE_NORMAL:
    {
      return (const char *) "NORMAL";
    }

    case LR20XX_RTTOF_MODE_EXTENDED:
    {
      return (const char *) "EXTENDED";
    }

    default:
    {
      return (const char *) "Unknown";
    }

  }

}

const char *lr20xx_rttof_spy_mode_to_str(const lr20xx_rttof_spy_mode_t value)
{
  switch (value)
  {
    case LR20XX_RTTOF_SPY_MODE_DISABLED:
    {
      return (const char *) "DISABLED";
    }

    case LR20XX_RTTOF_SPY_MODE_ENABLED:
    {
      return (const char *) "ENABLED";
    }

    default:
    {
      return (const char *) "Unknown";
    }

  }

}

const char *lr20xx_rttof_timing_synchronization_role_to_str(const lr20xx_rttof_timing_synchronization_role_t value)
{
  switch (value)
  {
    case LR20XX_RTTOF_TIMING_SYNCHRONIZATION_DISABLE:
    {
      return (const char *) "DISABLE";
    }

    case LR20XX_RTTOF_TIMING_SYNCHRONIZATION_INITIATOR:
    {
      return (const char *) "INITIATOR";
    }

    case LR20XX_RTTOF_TIMING_SYNCHRONIZATION_RESPONDER:
    {
      return (const char *) "RESPONDER";
    }

    default:
    {
      return (const char *) "Unknown";
    }

  }

}

const char *lr20xx_rttof_dio_to_str(const lr20xx_rttof_dio_t value)
{
  switch (value)
  {
    case LR20XX_RTTOF_DIO_5:
    {
      return (const char *) "5";
    }

    case LR20XX_RTTOF_DIO_6:
    {
      return (const char *) "6";
    }

    case LR20XX_RTTOF_DIO_7:
    {
      return (const char *) "7";
    }

    case LR20XX_RTTOF_DIO_8:
    {
      return (const char *) "8";
    }

    case LR20XX_RTTOF_DIO_9:
    {
      return (const char *) "9";
    }

    case LR20XX_RTTOF_DIO_10:
    {
      return (const char *) "10";
    }

    case LR20XX_RTTOF_DIO_11:
    {
      return (const char *) "11";
    }

    default:
    {
      return (const char *) "Unknown";
    }

  }

}

