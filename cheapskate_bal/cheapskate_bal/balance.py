"""
This module contains the logic blocks for the balancer.
Stuff like file reading, data manipulation, graphing, etc
"""

import numpy as np
import pandas as pd
import pdb
import matplotlib.pyplot as plt
from scipy.signal import butter, lfilter

def read_data_files(stem, freq, samp_rate):
  fnamech1 = stem + '-ch1.csv'
  fnamech2 = stem + '-ch2.csv'
  fnamerpm = stem + '-rpm.csv'
  samp_ns = 1000000000/samp_rate;
  samp_str = '{0}ns'.format(samp_ns)

  ch1frame = pd.read_csv(filepath_or_buffer=fnamech1, header=None, names=["ts", "ch1"], dtype={'ts':np.int64, 'ch1':np.int32})
  ch2frame = pd.read_csv(filepath_or_buffer=fnamech2, header=None, names=["ts", "ch2"], dtype={'ts':np.int64, 'ch2':np.int32})
  rpmframe = pd.read_csv(filepath_or_buffer=fnamerpm, header=None, names=["ts", "rpm"], dtype={'ts':np.int64, 'rpm':np.int32})

  #drop the last row, as it is often an outlier
  ch1frame.drop(ch1frame.tail(1).index, inplace=True)
  ch2frame.drop(ch2frame.tail(1).index, inplace=True)
  rpmframe.drop(rpmframe.tail(1).index, inplace=True)

  #set up our low pass filter, which we will apply to the data
  nyq = samp_rate * 0.5
  normal_cutoff = (freq*1.5) / nyq
  b,a = butter(6, normal_cutoff, btype='low', analog=False)

  #rpm is active-low -- find the first high to low transition and take that as the 0 deg point

  trig = rpmframe['rpm'].diff();
  start_time = rpmframe.ts.iloc[rpmframe[trig.gt(50)].index[0]] #this is crap -- it looks like matlab (ts of first row where trig > 50)

  #remove data before the 0 deg point and 
  #resample the frames, starting at start_time
  end_time = (ch1frame.ts.iloc[-1] + ch2frame.ts.iloc[-1] + rpmframe.ts.iloc[-1])/3 - start_time;
  newIndex = pd.timedelta_range(start='0ns', periods=end_time/samp_ns, freq=samp_str)

  out = []
  for f in [ch1frame, ch2frame, rpmframe]:
     f.drop(f[f.ts <= start_time].index, inplace=True)
     f['t'] = pd.to_timedelta(f.ts - start_time, unit='ns')
     f.set_index('t', inplace=True)
     f.drop(columns='ts', inplace=True)
     dummy_frame = pd.DataFrame(np.NaN, index=newIndex, columns=f.columns)
     out.append(dummy_frame.combine_first(f).interpolate('time').resample(samp_str).asfreq().fillna(method='ffill').fillna(method='bfill'))

  df=pd.concat(out, axis=1).fillna(method='ffill')
  for col in df:
      df[col] = lfilter(b,a,df[col]) #TODO: probably want a bandpass filter here rather than just lowpass
  return df

def graph_data(df):
  df.plot()
  plt.show()

def process_data(df,freq,samp_rate):
    out = {}
    for d in [df.ch1, df.ch2, df.rpm]:
        #compute fft
        fft= np.fft.rfft(d-np.mean(d)) #de-mean so we don't get a big dc value
        #show graph
        frqs = np.fft.rfftfreq(d.size, d=1.0/samp_rate)
        #plt.plot(frqs, fft.real**2 + fft.imag**2)
        #plt.show()
        maxfidx = np.argmax(np.abs(fft))
        maxf = frqs[maxfidx]
        print("Max frequency at {0} Hz ({1} RPM)".format(maxf, maxf*60))
        #extract real and imaginary part  #TODO: should constrain this to be at or near the frequency of interest
        outval = fft[maxfidx]
        out[d.name] = {'maxfft':outval, 'amplitude':np.abs(outval)}
    print(out)
    return out

def single_balance(results, tmass, shift_ang, offset_1_ang, offset_2_ang):
    x_res = ((2*results[1]['ch1']['amplitude']**2)-(results[2]['ch1']['amplitude']**2)-(results[3]['ch1']['amplitude']**2))/((4*results[0]['ch1']['amplitude']**2)*(1-np.cos(np.radians(shift_ang))))
    y_res = ((results[2]['ch1']['amplitude']**2)-(results[3]['ch1']['amplitude']**2))/((4*results[0]['ch1']['amplitude']**2)*(np.sin(np.radians(shift_ang))))
    mass_ratio = 1.0/(np.linalg.norm([x_res, y_res]))
    add_mass = tmass * mass_ratio;
    remove_ang = np.degrees(np.arctan2(y_res, x_res)) #note that numpy atan2 uses the (y,x) convention
    add_ang = 180 if remove_ang == 0 else -1*np.sign(remove_ang)*(180-np.abs(remove_ang))
    offset_mass1 = add_mass*((np.sin(np.radians(offset_2_ang)))/(np.sin(np.radians(offset_1_ang+offset_2_ang))))
    offset_mass2 = add_mass*((np.sin(np.radians(offset_1_ang)))/(np.sin(np.radians(offset_1_ang+offset_2_ang))))
    #that's it for the math, now print the report
    print("x result: {}".format(x_res))
    print("y result: {}".format(y_res))
    print("U/T ratio of unbalance to test mass: {}".format(mass_ratio))
    print("material to be removed or added: {}".format(add_mass))
    print("Angle of material removal: {}".format(remove_ang))
    print(" Or, angle of material addition: {}".format(add_ang))
    print("  Offset mass 1: {}".format(offset_mass1))
    print("  Offset mass 2: {}".format(offset_mass2))

    
#dual plane balancing functions
#  Variables:
#    V10 = vibration at b1 during trial 0
#    V20 = vibration at b2 during trial 0
#
#    TU1A = trial weight added at b1 for trial A
#    V1A = vibration at b1 during trial A    
#    V2A = vibration at b2 during trial A
#
#    TU2B = trial weight added at b2 for trial B
#    V1B = vibration at b1 during trial B
#    V2B = vibration at b2 during trial B
#
#    (these are computed)
#    U1 = correction weight at b1
#    U2 = correction weight at b2
#
#    K11 = influence
#    K12 = influence
#    K21 = influence
#    K22 = influence
#
#    Note that for the purposes of this program, we assume that TU1A == TU2B (test weights are equal and positioned at the same angle for trial A and B)
#    Also note that computations are done in the imaginary plane, but the test weights are input as magnitude, angle (see conversion below)

#    Equasions:
#        U2 = (-K11*V20+V10*K12)/(-K11*K22+K21*K12)  U1=(K22*V10-V20*K21)/(-K11*K22+K21*K12)
#        K11 = (V1A-V10)/TU1A
#        K12 = (V2A-V20)/TU1A
#        K21 = (V1B-V10)/TU2B
#        K22 = (V2B-V20)/TU2B
#
#    Note that U1,U2 can be recomputed from a single (T0) once the influence parameters Kmn are known
#

#take ang/mag and return imaginary plane coordinate (r is scalar, theta angle in degrees)
def from_angmag(r,theta):
    return r * np.exp( 1j * np.radians(theta) )

#take imaginary coordinate, return angle (in degrees) and magnitude
def to_angmag(z):
    return (np.abs(z), np.degrees(np.angle(z)))

def dual_compute_weights(results, influence):
    V10 = results[0]['ch1']['maxfft']
    V20 = results[0]['ch2']['maxfft']
    K11,K12,K21,K22 = influence

    U1 = (K22*V10-V20*K21)/(-K11*K22+K21*K12)
    U2 = (-K11*V20+V10*K12)/(-K11*K22+K21*K12)

    u1out=to_angmag(U1)
    u2out=to_angmag(U2)

    print("U1: {0} mass at {1} degrees".format(*u1out))
    print("U2: {0} mass at {1} degrees".format(*u2out))
    return (U1,U2)

def dual_compute_influence(results, tmass, shift_ang):

    V10 = results[0]['ch1']['maxfft']
    V20 = results[0]['ch2']['maxfft']

    TU1A = from_angmag(tmass, shift_ang)
    V1A = results[1]['ch1']['maxfft']
    V2A = results[1]['ch2']['maxfft']

    TU2B = TU1A
    V1B = results[3]['ch1']['maxfft']
    V2B = results[3]['ch2']['maxfft']

    #compute influence
    K11 = (V1A-V10)/TU1A
    K12 = (V2A-V20)/TU1A
    K21 = (V1B-V10)/TU2B
    K22 = (V2B-V20)/TU2B
    
    influence = [K11, K12, K21, K22]

    correction = dual_compute_weights(results, influence)

    return (influence, correction)


