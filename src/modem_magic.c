/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "math.h"
#include "bs_types.h"
#include "bs_tracing.h"
#include "bs_oswrap.h"
#include "bs_utils.h"
#include "bs_pc_2G4.h"
#include "bs_pc_2G4_utils.h"
#include "modem_if.h"
#include "modem_magic_args.h"
#include "p2G4_pending_tx_rx_list.h"

/**
 * Initialize the modem internal status
 *
 * The input parameters are:
 *   dev_nbr      : the device number this modem library corresponds to
 *                  (note that this library may be reused in between several devices)
 *   nbr_devices  : the total number of devices in the simulation (gives the length of vectors later)
 *   char *argv[] : a set of arguments passed to the modem library (same convention as POSIX main() command line arguments)
 *   int argc     : the number of arguments passed to the modem library
 *
 * See modem_magic_argparse() for more info about this parameters
 *
 * This function returns a pointer to this modem's status structure
 * This same pointer will be passed back to consecutive function calls of the library (void *this)
 */
void* modem_init(int argc, char *argv[], uint dev_nbr, uint nbr_devices) {
  mo_magic_args_t *args;

  args = (mo_magic_args_t *)bs_calloc(1,sizeof(mo_magic_args_t));
  args->nbr_devices = nbr_devices;

  modem_magic_argparse(argc, argv, dev_nbr, args);

  return (void*)args;
}

/**
 * Calculate the SNR of the desired input signal at the digital modem input/analog output
 * including all modem analog impairments
 *
 * inputs:
 *  this             : Pointer to this modem object
 *  rx_radio_params  : Radio parameters/configuration of this receiver for this Rx/RSSI measurement
 *  rx_powers        : For each possible transmitter ([0..n_devices]) what is their power level at this device antenna connector
 *  txl_c            : For each possible transmitter what are their Tx parameters
 *  tx_nbr           : which of the transmitters is the one we are trying to receive
 *
 * outputs:
 *  OutputSNR               : SNR in the analog output / digital input of this modem (dB)
 *  Output_RSSI_power_level : RSSI level (analog, dBm) sent to the modem digital
 */
void modem_analog_rx(void *this, p2G4_radioparamsv2_t *rx_radio_params, double *OutputSNR,double *Output_RSSI_power_level,
                     double *rx_powers, tx_l_c_t *txl_c, uint tx_nbr) {

  mo_magic_args_t *mo_st = (mo_magic_args_t *)this;

  //Trivial RSSI model: If a transmitted has the same center frequency as where the receiver is open,
  //we assume all its power is measured. Otherwise none.
  //Meaning, we ignore receiver filter and modulation shapes
  p2G4_freq2_t rx_freq = rx_radio_params->center_freq;
  double acc_power = 4e-12; //-114dBm : Thermal noise floor for 1MHz @300K
  for (int i = 0; i < mo_st->nbr_devices ; i++ ) {
    if ( txl_c->used[i] ){ //For each active transmitter (including the desired one
      if (rx_freq == txl_c->tx_list[i].tx_s.radio_params.center_freq) {
        acc_power += pow(10.0, rx_powers[i]/10.0);
      }
    }
  }

  *Output_RSSI_power_level = 10*log10(acc_power);
  *OutputSNR = 100;

}

/**
 * Return the bit error probability ([0.. RAND_PROB_1]) for a given SNR
 *
 * inputs:
 *  this          : Pointer to this modem object
 *  rx_modem_params: Radio parameters/configuration of this receiver for this Rx/RSSI measurement
 *  SNR           : SNR level at the analog output as calculated by modem_analog_rx()
 */
uint32_t modem_digital_perf_ber(void *this, p2G4_modemdigparams_t *rx_modem_params, double SNR) {
  return ((mo_magic_args_t*)this)->BER;
}

/**
 * Return the probability of the packet sync'ing ([0.. RAND_PROB_1]) for a given SNR and
 * transmission parameters
 *
 * (note that this should ONLY include excess packet error probability,
 * as over the sync word and address normal bit errors will also be calculated)
 *
 * inputs:
 *  this            : Pointer to this modem object
 *  rx_modem_params : Radio parameters/configuration of this receiver for this Rx/RSSI measurement
 *  SNR             : SNR level at the analog output as calculated by modem_analog_rx()
 *  tx_s            : Parameters of the transmission we are receiving (in case the sync. probability depends on any of them)
 */
uint32_t modem_digital_perf_sync(void *this, p2G4_modemdigparams_t *rx_modem_params, double SNR, p2G4_tx2v1_t* tx_s) {
  return  ((mo_magic_args_t*)this)->sync_prob;
}

/**
 * Return the digital RSSI value the modem would have produced for this given
 * RSSI_power_level in the digital input
 *
 * inputs:
 *  this             : Pointer to this modem object
 *  rx_radio_params  : Radio parameters/configuration of this receiver for this Rx/RSSI measurement
 *  RSSI_power_level : Analog power level as measured by modem_analog_rx()
 *
 * outputs:
 *  RSSI  : RSSI "digital" value returned by this modem, following the c2G4_rssi_power_t format (16.16 signed value)
 */
void modem_digital_RSSI(void *this, p2G4_radioparamsv2_t *rx_radio_params, double RSSI_power_level,
                        p2G4_rssi_power_t *RSSI) {

  *RSSI = p2G4_RSSI_value_from_dBm(RSSI_power_level);
}

/**
 * Clean up: Free the memory the modem may have allocated
 * close any file descriptors etc.
 * (the simulation has ended)
 */
void modem_delete(void *this){
  if (this != NULL){
    free(this);
  }
}

