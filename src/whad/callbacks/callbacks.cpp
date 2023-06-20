#include "whad/callbacks/callbacks.h"

/**
 * @brief Generic verbose message encoding callback.
 *
 * @param ostream Output stream
 * @param field Pointer to a field descriptor.
 * @param arg Pointer to a custom argument storing a pointer onto the text message to encode.
 * @return true if everything went ok.
 * @return false if an error is encountered during encoding.
 */
bool whad_verbose_msg_encode_cb(pb_ostream_t *ostream, const pb_field_t *field, void * const *arg)
{
    /* Take arg and encode it. */
    char *psz_message = *(char **)arg;
    int message_length = strlen(psz_message);

    if (ostream != NULL && field->tag == generic_VerboseMsg_data_tag)
    {
        /* This encodes the header for the field, based on the constant info
        * from pb_field_t. */
        if (!pb_encode_tag_for_field(ostream, field))
            return false;

        pb_encode_string(ostream, (pb_byte_t *)psz_message, message_length);
    }

    return true;
}

bool whad_disc_enum_capabilities_cb(pb_ostream_t *ostream, const pb_field_t *field, void * const *arg)
{
    DeviceCapability *capabilities = *(DeviceCapability **)arg;
    if (ostream != NULL && field->tag == discovery_DeviceInfoResp_capabilities_tag)
    {
        while ((capabilities->cap != 0) && (capabilities->domain != 0))
        {
            if (!pb_encode_tag_for_field(ostream, field))
                return false;

            if (!pb_encode_varint(ostream, capabilities->domain | capabilities->cap))
                return false;

            /* Go to next capability. */
            capabilities++;
        }
    }

    return true;
}


bool whad_phy_frequency_range_encode_cb(pb_ostream_t *ostream, const pb_field_t *field, void * const *arg)
{
  phy_SupportedFrequencyRanges_FrequencyRange *frequency_range = *(phy_SupportedFrequencyRanges_FrequencyRange **)arg;
  if (ostream != NULL && field->tag == phy_SupportedFrequencyRanges_frequency_ranges_tag)
  {
    while ((frequency_range->start != 0) && (frequency_range->end != 0))
    {
      if (!pb_encode_tag_for_field(ostream, field))
      {
          const char * error = PB_GET_ERROR(ostream);
          PB_UNUSED(error);
          return false;
      }

      if (!pb_encode_submessage(ostream, phy_SupportedFrequencyRanges_FrequencyRange_fields, frequency_range))
      {
          const char * error = PB_GET_ERROR(ostream);
          PB_UNUSED(error);
          return false;
      }
      frequency_range++;
    }
  }
  return true;
}
