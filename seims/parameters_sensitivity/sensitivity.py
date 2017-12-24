#! /usr/bin/env python
# -*- coding: utf-8 -*-
"""Base class of parameters sensitivity analysis.
    @author   : Liangjun Zhu
    @changelog: 17-12-22  lj - initial implementation.\n
"""
import os
import sys

if os.path.abspath(os.path.join(sys.path[0], '..')) not in sys.path:
    sys.path.append(os.path.abspath(os.path.join(sys.path[0], '..')))

import matplotlib

if os.name != 'nt':  # Force matplotlib to not use any Xwindows backend.
    matplotlib.use('Agg', warn=False)
import matplotlib.pyplot as plt

import numpy
from pygeoc.utils import get_config_parser
from SALib.sample.morris import sample as morris_spl
from SALib.analyze.morris import analyze as morris_alz
from SALib.plotting.morris import horizontal_bar_plot, covariance_plot, sample_histograms

from preprocess.db_mongodb import ConnectMongoDB
from config import PSAConfig
from preprocess.utility import read_data_items_from_txt
from evaluate import evaluate_model_response, build_seims_model


class Sensitivity(object):
    """Base class of Sensitivity Analysis."""

    def __init__(self, psa_cfg):
        """
        Initialization.
        Args:
            psa_cfg: PSAConfig object.
        """
        self.cfg = psa_cfg
        self.param_defs = dict()
        self.param_values = None
        self.run_count = 0
        self.output_nameIndex = dict()
        self.output_values = list()
        self.morris_si = dict()

    def read_param_ranges(self):
        """Read param_rng.def file

           name,lower_bound,upper_bound,group,dist
           (group and dist are optional)

            e.g.,
             Param1,0,1[,Group1][,dist1]
             Param2,0,1[,Group2][,dist2]
             Param3,0,1[,Group3][,dist3]

        Returns:
            a dictionary containing:
            - names - the names of the parameters
            - bounds - a list of lists of lower and upper bounds
            - num_vars - a scalar indicating the number of variables
                         (the length of names)
            - groups - a list of group names (strings) for each variable
            - dists - a list of distributions for the problem,
                        None if not specified or all uniform
        """
        client = ConnectMongoDB(self.cfg.hostname, self.cfg.port)
        conn = client.get_conn()
        db = conn[self.cfg.spatial_db]
        collection = db['PARAMETERS']

        names = list()
        bounds = list()
        groups = list()
        dists = list()
        num_vars = 0
        items = read_data_items_from_txt(self.cfg.param_range_def)
        for item in items:
            if len(item) < 3:
                continue
            # find parameter name, print warning message if not existed
            cursor = collection.find({'NAME': item[0]}, no_cursor_timeout=True)
            if not cursor.count():
                print ('WARNING: parameter %s is not existed!' % item[0])
                continue
            num_vars += 1
            names.append(item[0])
            bounds.append([float(item[1]), float(item[2])])
            # If the fourth column does not contain a group name, use
            # the parameter name
            if len(item) >= 4:
                groups.append(item[3])
            else:
                groups.append(item[0])
            if len(item) >= 5:
                dists.append(item[4])
            else:
                dists.append('unif')
        if groups == names:
            groups = None
        elif len(set(groups)) == 1:
            raise ValueError('''Only one group defined, results will not be
                    meaningful''')

        # setting dists to none if all are uniform
        # because non-uniform scaling is not needed
        if all([d == 'unif' for d in dists]):
            dists = None

        self.param_defs = {'names': names, 'bounds': bounds,
                           'num_vars': num_vars, 'groups': groups, 'dists': dists}
        # Save as txt file
        with open(self.cfg.psa_outpath + os.sep + 'param_defs.txt', 'w') as f:
            f.write(self.param_defs.__str__())

    def generate_samples(self):
        """Sampling and write to a single file and MongoDB 'PARAMETERS' collection"""
        self.param_values = morris_spl(self.param_defs, self.cfg.N,
                                       self.cfg.num_levels, self.cfg.grid_jump,
                                       optimal_trajectories=self.cfg.optimal_t,
                                       local_optimization=self.cfg.local_opt)
        self.run_count = len(self.param_values)
        # Save as txt file
        with open(self.cfg.psa_outpath + os.sep + 'param_values.txt', 'w') as f:
            f.write(self.param_values.__str__())
        # Plots a set of subplots of histograms of the input sample
        histfig = plt.figure()
        sample_histograms(histfig, self.param_values, self.param_defs, {'color': 'y'})
        plt.savefig(self.cfg.psa_outpath + os.sep + 'samples_histgram.png', dpi=300)
        # close current plot in case of 'figure.max_open_warning'
        plt.cla()
        plt.clf()
        plt.close()

    def write_param_values_to_mongodb(self):
        # update Parameters collection in MongoDB
        client = ConnectMongoDB(self.cfg.hostname, self.cfg.port)
        conn = client.get_conn()
        db = conn[self.cfg.spatial_db]
        collection = db['PARAMETERS']
        for idx, pname in enumerate(self.param_defs['names']):
            v2str = ','.join(str(v) for v in self.param_values[:, idx])
            collection.find_one_and_update({'NAME': pname}, {'$set': {'CALI_VALUES': v2str}})
        client.close()

    def evaluate(self):
        """Run SEIMS for objective output variables, and write out.
        """
        # TODO, need to think about a elegant way to define and calculate ouput variables.
        self.output_nameIndex = {'Q': [0, ' ($m^3/s$)'], 'SED': [1, ' ($10^3 ton$)']}

        cali_seqs = range(self.run_count)
        model_cfg_dict = {'bin_dir': self.cfg.seims_bin, 'model_dir': self.cfg.model_dir,
                          'nthread': self.cfg.seims_nthread, 'lyrmethod': self.cfg.seims_lyrmethod,
                          'hostname': self.cfg.hostname, 'port': self.cfg.port,
                          'scenario_id': 0}
        cali_models = map(build_seims_model, [model_cfg_dict] * self.run_count, cali_seqs)
        try:
            # parallel on multiprocesor or clusters using SCOOP
            from scoop import futures
            self.output_values = futures.map(evaluate_model_response, cali_models)
        except ImportError or ImportWarning:
            # serial
            self.output_values = map(evaluate_model_response, cali_models)
        # print (self.output_values)
        # Save as txt file
        with open(self.cfg.psa_outpath + os.sep + 'output_values.txt', 'w') as f:
            f.write(self.output_values.__str__())

    def calc_elementary_effects(self):
        """Calculate Morris elementary effects.
           It is worth to be noticed that evaluate() allows to return several output variables,
           hence we should calculate each of them separately.
        """
        out_values = numpy.array(self.output_values)
        # self.output_nameIndex = {'Q': [0, ' ($m^3/s$)'], 'SED': [1, ' ($10^3 ton$)']}
        # out_values = [[53.20732505000001, 323064.58129882975], [54.35469497000004, 331747.6614838499], [54.46669122999999, 1009158.6627484204], [54.61147778, 1242234.4139392804], [54.084982819999986, 1229273.6740602402], [46.20190839000001, 1147618.83466198], [54.38911660999996, 1021355.3732644306], [54.017732579999986, 1221556.3489041997], [54.54001437999998, 1224883.2074504197], [54.02695252000005, 1211234.6394682492], [54.196631449999984, 1238477.09017777], [54.4356827, 1239976.296446429], [53.9237271, 763266.8212967201], [54.16514828000002, 115106.22616571006], [54.1645179, 330731.1208496102], [53.89117702, 328551.50125130004], [54.42788821999994, 331913.7325134796], [54.376645529999955, 1231917.8782630202], [54.846279120000034, 1035188.4677792294], [53.03035475000002, 999222.6287994498], [53.598714570000006, 862156.71859744], [53.86588150000003, 515381.69465637975], [53.534163789999965, 506770.73594673973], [54.30185833999998, 1221047.4829425495], [53.67261549000003, 72553.29424197007], [53.945973700000025, 72724.80694776004], [54.58844332000002, 75790.01643939005], [54.54594420999998, 1238413.6581869302], [54.64988468000002, 1242061.79801362], [54.67266801999998, 1245126.52196492], [45.90096921999997, 58386.78985123999], [45.78781508, 1118332.74664709], [54.204704909999975, 1245867.1212670803], [54.54516418, 1232142.0335033198], [54.36687455999997, 1240957.5398087797], [54.180190110000034, 1249256.3081657898], [53.960694090000025, 1228518.6437183698], [54.16404042999999, 1255645.608881399], [54.445512770000015, 1248264.30901122], [53.43886268000001, 1210406.1455768405], [53.40006645000002, 1207912.2391449397], [45.14396151999998, 1087082.7214320002], [55.13190495999999, 1270163.4714062095], [45.49643396999999, 1113216.7020750805], [44.84661235999997, 1078306.3550340398], [45.68742247, 1090722.3513139402], [44.955692099999986, 1058926.8391672503], [45.09285096999997, 55181.77286720999], [45.44381009, 1101817.9950276301], [45.60303295, 1104144.9549065505], [54.040593150000014, 1231763.1505409288], [54.25106206000002, 1244884.24333326], [53.27454116999999, 1194896.46516085], [53.63152406000001, 1200574.6655430994], [54.08857335000001, 1224579.4653402301], [54.46882092000003, 1255378.668844419], [54.30233799, 1168075.0955953202], [54.3226233, 1242509.9160931597], [54.65549266999997, 1252925.4758922197], [45.45966362999997, 1081874.65559343]]
        # out_values = numpy.array(out_values)
        for k, v in self.output_nameIndex.iteritems():
            print (k)
            tmp_Si = morris_alz(self.param_defs,
                                self.param_values,
                                out_values[:, v[0]],
                                conf_level=0.95, print_to_console=True,
                                num_levels=self.cfg.num_levels,
                                grid_jump=self.cfg.grid_jump)
            self.morris_si[k] = tmp_Si
            fig, (ax1, ax2) = plt.subplots(1, 2)
            horizontal_bar_plot(ax1, tmp_Si, {}, sortby='mu_star', unit=v[1])
            covariance_plot(ax2, tmp_Si, {}, unit=v[1])
            plt.savefig('%s/mu_star_%s.png' % (self.cfg.psa_outpath, k), dpi=300)
            # plt.show()
            # close current plot in case of 'figure.max_open_warning'
            plt.cla()
            plt.clf()
            plt.close()
        print (self.morris_si)


if __name__ == '__main__':
    cf = get_config_parser()
    cfg = PSAConfig(cf)
    saobj = Sensitivity(cfg)
    saobj.read_param_ranges()
    saobj.generate_samples()

    print (saobj.param_defs)
    print (len(saobj.param_values))
