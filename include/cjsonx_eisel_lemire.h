/**
 * @file cjsonx_eisel_lemire.h
 * @brief Precomputed tables for Eisel-Lemire float conversion
 *
 * @note Architecture and coding style inspired by yyjson (https://github.com/ibireme/yyjson)
 */
#ifndef CJSONX_EISEL_LEMIRE_H
#define CJSONX_EISEL_LEMIRE_H

/*==============================================================================
 * MARK: - eisel-lemire float parsing
 *============================================================================*/


#include <stdint.h>

static const uint64_t cjsonx_eisel_lemire_mantissa[] = {
    0xfa8fd5a0081c0288ULL, // 10^-348
    0x9c99e58405118195ULL, // 10^-347
    0xc3c05ee50655e1faULL, // 10^-346
    0xf4b0769e47eb5a79ULL, // 10^-345
    0x98ee4a22ecf3188cULL, // 10^-344
    0xbf29dcaba82fdeaeULL, // 10^-343
    0xeef453d6923bd65aULL, // 10^-342
    0x9558b4661b6565f8ULL, // 10^-341
    0xbaaee17fa23ebf76ULL, // 10^-340
    0xe95a99df8ace6f54ULL, // 10^-339
    0x91d8a02bb6c10594ULL, // 10^-338
    0xb64ec836a47146faULL, // 10^-337
    0xe3e27a444d8d98b8ULL, // 10^-336
    0x8e6d8c6ab0787f73ULL, // 10^-335
    0xb208ef855c969f50ULL, // 10^-334
    0xde8b2b66b3bc4724ULL, // 10^-333
    0x8b16fb203055ac76ULL, // 10^-332
    0xaddcb9e83c6b1794ULL, // 10^-331
    0xd953e8624b85dd79ULL, // 10^-330
    0x87d4713d6f33aa6cULL, // 10^-329
    0xa9c98d8ccb009506ULL, // 10^-328
    0xd43bf0effdc0ba48ULL, // 10^-327
    0x84a57695fe98746dULL, // 10^-326
    0xa5ced43b7e3e9188ULL, // 10^-325
    0xcf42894a5dce35eaULL, // 10^-324
    0x818995ce7aa0e1b2ULL, // 10^-323
    0xa1ebfb4219491a1fULL, // 10^-322
    0xca66fa129f9b60a7ULL, // 10^-321
    0xfd00b897478238d1ULL, // 10^-320
    0x9e20735e8cb16382ULL, // 10^-319
    0xc5a890362fddbc63ULL, // 10^-318
    0xf712b443bbd52b7cULL, // 10^-317
    0x9a6bb0aa55653b2dULL, // 10^-316
    0xc1069cd4eabe89f9ULL, // 10^-315
    0xf148440a256e2c77ULL, // 10^-314
    0x96cd2a865764dbcaULL, // 10^-313
    0xbc807527ed3e12bdULL, // 10^-312
    0xeba09271e88d976cULL, // 10^-311
    0x93445b8731587ea3ULL, // 10^-310
    0xb8157268fdae9e4cULL, // 10^-309
    0xe61acf033d1a45dfULL, // 10^-308
    0x8fd0c16206306bacULL, // 10^-307
    0xb3c4f1ba87bc8697ULL, // 10^-306
    0xe0b62e2929aba83cULL, // 10^-305
    0x8c71dcd9ba0b4926ULL, // 10^-304
    0xaf8e5410288e1b6fULL, // 10^-303
    0xdb71e91432b1a24bULL, // 10^-302
    0x892731ac9faf056fULL, // 10^-301
    0xab70fe17c79ac6caULL, // 10^-300
    0xd64d3d9db981787dULL, // 10^-299
    0x85f0468293f0eb4eULL, // 10^-298
    0xa76c582338ed2622ULL, // 10^-297
    0xd1476e2c07286faaULL, // 10^-296
    0x82cca4db847945caULL, // 10^-295
    0xa37fce126597973dULL, // 10^-294
    0xcc5fc196fefd7d0cULL, // 10^-293
    0xff77b1fcbebcdc4fULL, // 10^-292
    0x9faacf3df73609b1ULL, // 10^-291
    0xc795830d75038c1eULL, // 10^-290
    0xf97ae3d0d2446f25ULL, // 10^-289
    0x9becce62836ac577ULL, // 10^-288
    0xc2e801fb244576d5ULL, // 10^-287
    0xf3a20279ed56d48aULL, // 10^-286
    0x9845418c345644d7ULL, // 10^-285
    0xbe5691ef416bd60cULL, // 10^-284
    0xedec366b11c6cb8fULL, // 10^-283
    0x94b3a202eb1c3f39ULL, // 10^-282
    0xb9e08a83a5e34f08ULL, // 10^-281
    0xe858ad248f5c22caULL, // 10^-280
    0x91376c36d99995beULL, // 10^-279
    0xb58547448ffffb2eULL, // 10^-278
    0xe2e69915b3fff9f9ULL, // 10^-277
    0x8dd01fad907ffc3cULL, // 10^-276
    0xb1442798f49ffb4bULL, // 10^-275
    0xdd95317f31c7fa1dULL, // 10^-274
    0x8a7d3eef7f1cfc52ULL, // 10^-273
    0xad1c8eab5ee43b67ULL, // 10^-272
    0xd863b256369d4a41ULL, // 10^-271
    0x873e4f75e2224e68ULL, // 10^-270
    0xa90de3535aaae202ULL, // 10^-269
    0xd3515c2831559a83ULL, // 10^-268
    0x8412d9991ed58092ULL, // 10^-267
    0xa5178fff668ae0b6ULL, // 10^-266
    0xce5d73ff402d98e4ULL, // 10^-265
    0x80fa687f881c7f8eULL, // 10^-264
    0xa139029f6a239f72ULL, // 10^-263
    0xc987434744ac874fULL, // 10^-262
    0xfbe9141915d7a922ULL, // 10^-261
    0x9d71ac8fada6c9b5ULL, // 10^-260
    0xc4ce17b399107c23ULL, // 10^-259
    0xf6019da07f549b2bULL, // 10^-258
    0x99c102844f94e0fbULL, // 10^-257
    0xc0314325637a193aULL, // 10^-256
    0xf03d93eebc589f88ULL, // 10^-255
    0x96267c7535b763b5ULL, // 10^-254
    0xbbb01b9283253ca3ULL, // 10^-253
    0xea9c227723ee8bcbULL, // 10^-252
    0x92a1958a7675175fULL, // 10^-251
    0xb749faed14125d37ULL, // 10^-250
    0xe51c79a85916f485ULL, // 10^-249
    0x8f31cc0937ae58d3ULL, // 10^-248
    0xb2fe3f0b8599ef08ULL, // 10^-247
    0xdfbdcece67006ac9ULL, // 10^-246
    0x8bd6a141006042beULL, // 10^-245
    0xaecc49914078536dULL, // 10^-244
    0xda7f5bf590966849ULL, // 10^-243
    0x888f99797a5e012dULL, // 10^-242
    0xaab37fd7d8f58179ULL, // 10^-241
    0xd5605fcdcf32e1d7ULL, // 10^-240
    0x855c3be0a17fcd26ULL, // 10^-239
    0xa6b34ad8c9dfc070ULL, // 10^-238
    0xd0601d8efc57b08cULL, // 10^-237
    0x823c12795db6ce57ULL, // 10^-236
    0xa2cb1717b52481edULL, // 10^-235
    0xcb7ddcdda26da269ULL, // 10^-234
    0xfe5d54150b090b03ULL, // 10^-233
    0x9efa548d26e5a6e2ULL, // 10^-232
    0xc6b8e9b0709f109aULL, // 10^-231
    0xf867241c8cc6d4c1ULL, // 10^-230
    0x9b407691d7fc44f8ULL, // 10^-229
    0xc21094364dfb5637ULL, // 10^-228
    0xf294b943e17a2bc4ULL, // 10^-227
    0x979cf3ca6cec5b5bULL, // 10^-226
    0xbd8430bd08277231ULL, // 10^-225
    0xece53cec4a314ebeULL, // 10^-224
    0x940f4613ae5ed137ULL, // 10^-223
    0xb913179899f68584ULL, // 10^-222
    0xe757dd7ec07426e5ULL, // 10^-221
    0x9096ea6f3848984fULL, // 10^-220
    0xb4bca50b065abe63ULL, // 10^-219
    0xe1ebce4dc7f16dfcULL, // 10^-218
    0x8d3360f09cf6e4bdULL, // 10^-217
    0xb080392cc4349dedULL, // 10^-216
    0xdca04777f541c568ULL, // 10^-215
    0x89e42caaf9491b61ULL, // 10^-214
    0xac5d37d5b79b6239ULL, // 10^-213
    0xd77485cb25823ac7ULL, // 10^-212
    0x86a8d39ef77164bdULL, // 10^-211
    0xa8530886b54dbdecULL, // 10^-210
    0xd267caa862a12d67ULL, // 10^-209
    0x8380dea93da4bc60ULL, // 10^-208
    0xa46116538d0deb78ULL, // 10^-207
    0xcd795be870516656ULL, // 10^-206
    0x806bd9714632dff6ULL, // 10^-205
    0xa086cfcd97bf97f4ULL, // 10^-204
    0xc8a883c0fdaf7df0ULL, // 10^-203
    0xfad2a4b13d1b5d6cULL, // 10^-202
    0x9cc3a6eec6311a64ULL, // 10^-201
    0xc3f490aa77bd60fdULL, // 10^-200
    0xf4f1b4d515acb93cULL, // 10^-199
    0x991711052d8bf3c5ULL, // 10^-198
    0xbf5cd54678eef0b7ULL, // 10^-197
    0xef340a98172aace5ULL, // 10^-196
    0x9580869f0e7aac0fULL, // 10^-195
    0xbae0a846d2195713ULL, // 10^-194
    0xe998d258869facd7ULL, // 10^-193
    0x91ff83775423cc06ULL, // 10^-192
    0xb67f6455292cbf08ULL, // 10^-191
    0xe41f3d6a7377eecaULL, // 10^-190
    0x8e938662882af53eULL, // 10^-189
    0xb23867fb2a35b28eULL, // 10^-188
    0xdec681f9f4c31f31ULL, // 10^-187
    0x8b3c113c38f9f37fULL, // 10^-186
    0xae0b158b4738705fULL, // 10^-185
    0xd98ddaee19068c76ULL, // 10^-184
    0x87f8a8d4cfa417caULL, // 10^-183
    0xa9f6d30a038d1dbcULL, // 10^-182
    0xd47487cc8470652bULL, // 10^-181
    0x84c8d4dfd2c63f3bULL, // 10^-180
    0xa5fb0a17c777cf0aULL, // 10^-179
    0xcf79cc9db955c2ccULL, // 10^-178
    0x81ac1fe293d599c0ULL, // 10^-177
    0xa21727db38cb0030ULL, // 10^-176
    0xca9cf1d206fdc03cULL, // 10^-175
    0xfd442e4688bd304bULL, // 10^-174
    0x9e4a9cec15763e2fULL, // 10^-173
    0xc5dd44271ad3cdbaULL, // 10^-172
    0xf7549530e188c129ULL, // 10^-171
    0x9a94dd3e8cf578baULL, // 10^-170
    0xc13a148e3032d6e8ULL, // 10^-169
    0xf18899b1bc3f8ca2ULL, // 10^-168
    0x96f5600f15a7b7e5ULL, // 10^-167
    0xbcb2b812db11a5deULL, // 10^-166
    0xebdf661791d60f56ULL, // 10^-165
    0x936b9fcebb25c996ULL, // 10^-164
    0xb84687c269ef3bfbULL, // 10^-163
    0xe65829b3046b0afaULL, // 10^-162
    0x8ff71a0fe2c2e6dcULL, // 10^-161
    0xb3f4e093db73a093ULL, // 10^-160
    0xe0f218b8d25088b8ULL, // 10^-159
    0x8c974f7383725573ULL, // 10^-158
    0xafbd2350644eead0ULL, // 10^-157
    0xdbac6c247d62a584ULL, // 10^-156
    0x894bc396ce5da772ULL, // 10^-155
    0xab9eb47c81f5114fULL, // 10^-154
    0xd686619ba27255a3ULL, // 10^-153
    0x8613fd0145877586ULL, // 10^-152
    0xa798fc4196e952e7ULL, // 10^-151
    0xd17f3b51fca3a7a1ULL, // 10^-150
    0x82ef85133de648c5ULL, // 10^-149
    0xa3ab66580d5fdaf6ULL, // 10^-148
    0xcc963fee10b7d1b3ULL, // 10^-147
    0xffbbcfe994e5c620ULL, // 10^-146
    0x9fd561f1fd0f9bd4ULL, // 10^-145
    0xc7caba6e7c5382c9ULL, // 10^-144
    0xf9bd690a1b68637bULL, // 10^-143
    0x9c1661a651213e2dULL, // 10^-142
    0xc31bfa0fe5698db8ULL, // 10^-141
    0xf3e2f893dec3f126ULL, // 10^-140
    0x986ddb5c6b3a76b8ULL, // 10^-139
    0xbe89523386091466ULL, // 10^-138
    0xee2ba6c0678b597fULL, // 10^-137
    0x94db483840b717f0ULL, // 10^-136
    0xba121a4650e4ddecULL, // 10^-135
    0xe896a0d7e51e1566ULL, // 10^-134
    0x915e2486ef32cd60ULL, // 10^-133
    0xb5b5ada8aaff80b8ULL, // 10^-132
    0xe3231912d5bf60e6ULL, // 10^-131
    0x8df5efabc5979c90ULL, // 10^-130
    0xb1736b96b6fd83b4ULL, // 10^-129
    0xddd0467c64bce4a1ULL, // 10^-128
    0x8aa22c0dbef60ee4ULL, // 10^-127
    0xad4ab7112eb3929eULL, // 10^-126
    0xd89d64d57a607745ULL, // 10^-125
    0x87625f056c7c4a8bULL, // 10^-124
    0xa93af6c6c79b5d2eULL, // 10^-123
    0xd389b47879823479ULL, // 10^-122
    0x843610cb4bf160ccULL, // 10^-121
    0xa54394fe1eedb8ffULL, // 10^-120
    0xce947a3da6a9273eULL, // 10^-119
    0x811ccc668829b887ULL, // 10^-118
    0xa163ff802a3426a9ULL, // 10^-117
    0xc9bcff6034c13053ULL, // 10^-116
    0xfc2c3f3841f17c68ULL, // 10^-115
    0x9d9ba7832936edc1ULL, // 10^-114
    0xc5029163f384a931ULL, // 10^-113
    0xf64335bcf065d37dULL, // 10^-112
    0x99ea0196163fa42eULL, // 10^-111
    0xc06481fb9bcf8d3aULL, // 10^-110
    0xf07da27a82c37088ULL, // 10^-109
    0x964e858c91ba2655ULL, // 10^-108
    0xbbe226efb628afebULL, // 10^-107
    0xeadab0aba3b2dbe5ULL, // 10^-106
    0x92c8ae6b464fc96fULL, // 10^-105
    0xb77ada0617e3bbcbULL, // 10^-104
    0xe55990879ddcaabeULL, // 10^-103
    0x8f57fa54c2a9eab7ULL, // 10^-102
    0xb32df8e9f3546564ULL, // 10^-101
    0xdff9772470297ebdULL, // 10^-100
    0x8bfbea76c619ef36ULL, // 10^-99
    0xaefae51477a06b04ULL, // 10^-98
    0xdab99e59958885c5ULL, // 10^-97
    0x88b402f7fd75539bULL, // 10^-96
    0xaae103b5fcd2a882ULL, // 10^-95
    0xd59944a37c0752a2ULL, // 10^-94
    0x857fcae62d8493a5ULL, // 10^-93
    0xa6dfbd9fb8e5b88fULL, // 10^-92
    0xd097ad07a71f26b2ULL, // 10^-91
    0x825ecc24c8737830ULL, // 10^-90
    0xa2f67f2dfa90563bULL, // 10^-89
    0xcbb41ef979346bcaULL, // 10^-88
    0xfea126b7d78186bdULL, // 10^-87
    0x9f24b832e6b0f436ULL, // 10^-86
    0xc6ede63fa05d3144ULL, // 10^-85
    0xf8a95fcf88747d94ULL, // 10^-84
    0x9b69dbe1b548ce7dULL, // 10^-83
    0xc24452da229b021cULL, // 10^-82
    0xf2d56790ab41c2a3ULL, // 10^-81
    0x97c560ba6b0919a6ULL, // 10^-80
    0xbdb6b8e905cb600fULL, // 10^-79
    0xed246723473e3813ULL, // 10^-78
    0x9436c0760c86e30cULL, // 10^-77
    0xb94470938fa89bcfULL, // 10^-76
    0xe7958cb87392c2c3ULL, // 10^-75
    0x90bd77f3483bb9baULL, // 10^-74
    0xb4ecd5f01a4aa828ULL, // 10^-73
    0xe2280b6c20dd5232ULL, // 10^-72
    0x8d590723948a535fULL, // 10^-71
    0xb0af48ec79ace837ULL, // 10^-70
    0xdcdb1b2798182245ULL, // 10^-69
    0x8a08f0f8bf0f156bULL, // 10^-68
    0xac8b2d36eed2dac6ULL, // 10^-67
    0xd7adf884aa879177ULL, // 10^-66
    0x86ccbb52ea94baebULL, // 10^-65
    0xa87fea27a539e9a5ULL, // 10^-64
    0xd29fe4b18e88640fULL, // 10^-63
    0x83a3eeeef9153e89ULL, // 10^-62
    0xa48ceaaab75a8e2bULL, // 10^-61
    0xcdb02555653131b6ULL, // 10^-60
    0x808e17555f3ebf12ULL, // 10^-59
    0xa0b19d2ab70e6ed6ULL, // 10^-58
    0xc8de047564d20a8cULL, // 10^-57
    0xfb158592be068d2fULL, // 10^-56
    0x9ced737bb6c4183dULL, // 10^-55
    0xc428d05aa4751e4dULL, // 10^-54
    0xf53304714d9265e0ULL, // 10^-53
    0x993fe2c6d07b7facULL, // 10^-52
    0xbf8fdb78849a5f97ULL, // 10^-51
    0xef73d256a5c0f77dULL, // 10^-50
    0x95a8637627989aaeULL, // 10^-49
    0xbb127c53b17ec159ULL, // 10^-48
    0xe9d71b689dde71b0ULL, // 10^-47
    0x9226712162ab070eULL, // 10^-46
    0xb6b00d69bb55c8d1ULL, // 10^-45
    0xe45c10c42a2b3b06ULL, // 10^-44
    0x8eb98a7a9a5b04e3ULL, // 10^-43
    0xb267ed1940f1c61cULL, // 10^-42
    0xdf01e85f912e37a3ULL, // 10^-41
    0x8b61313bbabce2c6ULL, // 10^-40
    0xae397d8aa96c1b78ULL, // 10^-39
    0xd9c7dced53c72256ULL, // 10^-38
    0x881cea14545c7575ULL, // 10^-37
    0xaa242499697392d3ULL, // 10^-36
    0xd4ad2dbfc3d07788ULL, // 10^-35
    0x84ec3c97da624ab5ULL, // 10^-34
    0xa6274bbdd0fadd62ULL, // 10^-33
    0xcfb11ead453994baULL, // 10^-32
    0x81ceb32c4b43fcf5ULL, // 10^-31
    0xa2425ff75e14fc32ULL, // 10^-30
    0xcad2f7f5359a3b3eULL, // 10^-29
    0xfd87b5f28300ca0eULL, // 10^-28
    0x9e74d1b791e07e48ULL, // 10^-27
    0xc612062576589ddbULL, // 10^-26
    0xf79687aed3eec551ULL, // 10^-25
    0x9abe14cd44753b53ULL, // 10^-24
    0xc16d9a0095928a27ULL, // 10^-23
    0xf1c90080baf72cb1ULL, // 10^-22
    0x971da05074da7befULL, // 10^-21
    0xbce5086492111aebULL, // 10^-20
    0xec1e4a7db69561a5ULL, // 10^-19
    0x9392ee8e921d5d07ULL, // 10^-18
    0xb877aa3236a4b449ULL, // 10^-17
    0xe69594bec44de15bULL, // 10^-16
    0x901d7cf73ab0acd9ULL, // 10^-15
    0xb424dc35095cd80fULL, // 10^-14
    0xe12e13424bb40e13ULL, // 10^-13
    0x8cbccc096f5088ccULL, // 10^-12
    0xafebff0bcb24aaffULL, // 10^-11
    0xdbe6fecebdedd5bfULL, // 10^-10
    0x89705f4136b4a597ULL, // 10^-9
    0xabcc77118461cefdULL, // 10^-8
    0xd6bf94d5e57a42bcULL, // 10^-7
    0x8637bd05af6c69b6ULL, // 10^-6
    0xa7c5ac471b478423ULL, // 10^-5
    0xd1b71758e219652cULL, // 10^-4
    0x83126e978d4fdf3bULL, // 10^-3
    0xa3d70a3d70a3d70aULL, // 10^-2
    0xcccccccccccccccdULL, // 10^-1
    0x8000000000000000ULL, // 10^0
    0xa000000000000000ULL, // 10^1
    0xc800000000000000ULL, // 10^2
    0xfa00000000000000ULL, // 10^3
    0x9c40000000000000ULL, // 10^4
    0xc350000000000000ULL, // 10^5
    0xf424000000000000ULL, // 10^6
    0x9896800000000000ULL, // 10^7
    0xbebc200000000000ULL, // 10^8
    0xee6b280000000000ULL, // 10^9
    0x9502f90000000000ULL, // 10^10
    0xba43b74000000000ULL, // 10^11
    0xe8d4a51000000000ULL, // 10^12
    0x9184e72a00000000ULL, // 10^13
    0xb5e620f480000000ULL, // 10^14
    0xe35fa931a0000000ULL, // 10^15
    0x8e1bc9bf04000000ULL, // 10^16
    0xb1a2bc2ec5000000ULL, // 10^17
    0xde0b6b3a76400000ULL, // 10^18
    0x8ac7230489e80000ULL, // 10^19
    0xad78ebc5ac620000ULL, // 10^20
    0xd8d726b7177a8000ULL, // 10^21
    0x878678326eac9000ULL, // 10^22
    0xa968163f0a57b400ULL, // 10^23
    0xd3c21bcecceda100ULL, // 10^24
    0x84595161401484a0ULL, // 10^25
    0xa56fa5b99019a5c8ULL, // 10^26
    0xcecb8f27f4200f3aULL, // 10^27
    0x813f3978f8940984ULL, // 10^28
    0xa18f07d736b90be5ULL, // 10^29
    0xc9f2c9cd04674edfULL, // 10^30
    0xfc6f7c4045812296ULL, // 10^31
    0x9dc5ada82b70b59eULL, // 10^32
    0xc5371912364ce305ULL, // 10^33
    0xf684df56c3e01bc7ULL, // 10^34
    0x9a130b963a6c115cULL, // 10^35
    0xc097ce7bc90715b3ULL, // 10^36
    0xf0bdc21abb48db20ULL, // 10^37
    0x96769950b50d88f4ULL, // 10^38
    0xbc143fa4e250eb31ULL, // 10^39
    0xeb194f8e1ae525fdULL, // 10^40
    0x92efd1b8d0cf37beULL, // 10^41
    0xb7abc627050305aeULL, // 10^42
    0xe596b7b0c643c719ULL, // 10^43
    0x8f7e32ce7bea5c70ULL, // 10^44
    0xb35dbf821ae4f38cULL, // 10^45
    0xe0352f62a19e306fULL, // 10^46
    0x8c213d9da502de45ULL, // 10^47
    0xaf298d050e4395d7ULL, // 10^48
    0xdaf3f04651d47b4cULL, // 10^49
    0x88d8762bf324cd10ULL, // 10^50
    0xab0e93b6efee0054ULL, // 10^51
    0xd5d238a4abe98068ULL, // 10^52
    0x85a36366eb71f041ULL, // 10^53
    0xa70c3c40a64e6c52ULL, // 10^54
    0xd0cf4b50cfe20766ULL, // 10^55
    0x82818f1281ed44a0ULL, // 10^56
    0xa321f2d7226895c8ULL, // 10^57
    0xcbea6f8ceb02bb3aULL, // 10^58
    0xfee50b7025c36a08ULL, // 10^59
    0x9f4f2726179a2245ULL, // 10^60
    0xc722f0ef9d80aad6ULL, // 10^61
    0xf8ebad2b84e0d58cULL, // 10^62
    0x9b934c3b330c8577ULL, // 10^63
    0xc2781f49ffcfa6d5ULL, // 10^64
    0xf316271c7fc3908bULL, // 10^65
    0x97edd871cfda3a57ULL, // 10^66
    0xbde94e8e43d0c8ecULL, // 10^67
    0xed63a231d4c4fb27ULL, // 10^68
    0x945e455f24fb1cf9ULL, // 10^69
    0xb975d6b6ee39e437ULL, // 10^70
    0xe7d34c64a9c85d44ULL, // 10^71
    0x90e40fbeea1d3a4bULL, // 10^72
    0xb51d13aea4a488ddULL, // 10^73
    0xe264589a4dcdab15ULL, // 10^74
    0x8d7eb76070a08aedULL, // 10^75
    0xb0de65388cc8ada8ULL, // 10^76
    0xdd15fe86affad912ULL, // 10^77
    0x8a2dbf142dfcc7abULL, // 10^78
    0xacb92ed9397bf996ULL, // 10^79
    0xd7e77a8f87daf7fcULL, // 10^80
    0x86f0ac99b4e8dafdULL, // 10^81
    0xa8acd7c0222311bdULL, // 10^82
    0xd2d80db02aabd62cULL, // 10^83
    0x83c7088e1aab65dbULL, // 10^84
    0xa4b8cab1a1563f52ULL, // 10^85
    0xcde6fd5e09abcf27ULL, // 10^86
    0x80b05e5ac60b6178ULL, // 10^87
    0xa0dc75f1778e39d6ULL, // 10^88
    0xc913936dd571c84cULL, // 10^89
    0xfb5878494ace3a5fULL, // 10^90
    0x9d174b2dcec0e47bULL, // 10^91
    0xc45d1df942711d9aULL, // 10^92
    0xf5746577930d6501ULL, // 10^93
    0x9968bf6abbe85f20ULL, // 10^94
    0xbfc2ef456ae276e9ULL, // 10^95
    0xefb3ab16c59b14a3ULL, // 10^96
    0x95d04aee3b80ece6ULL, // 10^97
    0xbb445da9ca61281fULL, // 10^98
    0xea1575143cf97227ULL, // 10^99
    0x924d692ca61be758ULL, // 10^100
    0xb6e0c377cfa2e12eULL, // 10^101
    0xe498f455c38b997aULL, // 10^102
    0x8edf98b59a373fecULL, // 10^103
    0xb2977ee300c50fe7ULL, // 10^104
    0xdf3d5e9bc0f653e1ULL, // 10^105
    0x8b865b215899f46dULL, // 10^106
    0xae67f1e9aec07188ULL, // 10^107
    0xda01ee641a708deaULL, // 10^108
    0x884134fe908658b2ULL, // 10^109
    0xaa51823e34a7eedfULL, // 10^110
    0xd4e5e2cdc1d1ea96ULL, // 10^111
    0x850fadc09923329eULL, // 10^112
    0xa6539930bf6bff46ULL, // 10^113
    0xcfe87f7cef46ff17ULL, // 10^114
    0x81f14fae158c5f6eULL, // 10^115
    0xa26da3999aef774aULL, // 10^116
    0xcb090c8001ab551cULL, // 10^117
    0xfdcb4fa002162a63ULL, // 10^118
    0x9e9f11c4014dda7eULL, // 10^119
    0xc646d63501a1511eULL, // 10^120
    0xf7d88bc24209a565ULL, // 10^121
    0x9ae757596946075fULL, // 10^122
    0xc1a12d2fc3978937ULL, // 10^123
    0xf209787bb47d6b85ULL, // 10^124
    0x9745eb4d50ce6333ULL, // 10^125
    0xbd176620a501fc00ULL, // 10^126
    0xec5d3fa8ce427b00ULL, // 10^127
    0x93ba47c980e98ce0ULL, // 10^128
    0xb8a8d9bbe123f018ULL, // 10^129
    0xe6d3102ad96cec1eULL, // 10^130
    0x9043ea1ac7e41393ULL, // 10^131
    0xb454e4a179dd1877ULL, // 10^132
    0xe16a1dc9d8545e95ULL, // 10^133
    0x8ce2529e2734bb1dULL, // 10^134
    0xb01ae745b101e9e4ULL, // 10^135
    0xdc21a1171d42645dULL, // 10^136
    0x899504ae72497ebaULL, // 10^137
    0xabfa45da0edbde69ULL, // 10^138
    0xd6f8d7509292d603ULL, // 10^139
    0x865b86925b9bc5c2ULL, // 10^140
    0xa7f26836f282b733ULL, // 10^141
    0xd1ef0244af2364ffULL, // 10^142
    0x8335616aed761f1fULL, // 10^143
    0xa402b9c5a8d3a6e7ULL, // 10^144
    0xcd036837130890a1ULL, // 10^145
    0x802221226be55a65ULL, // 10^146
    0xa02aa96b06deb0feULL, // 10^147
    0xc83553c5c8965d3dULL, // 10^148
    0xfa42a8b73abbf48dULL, // 10^149
    0x9c69a97284b578d8ULL, // 10^150
    0xc38413cf25e2d70eULL, // 10^151
    0xf46518c2ef5b8cd1ULL, // 10^152
    0x98bf2f79d5993803ULL, // 10^153
    0xbeeefb584aff8604ULL, // 10^154
    0xeeaaba2e5dbf6785ULL, // 10^155
    0x952ab45cfa97a0b3ULL, // 10^156
    0xba756174393d88e0ULL, // 10^157
    0xe912b9d1478ceb17ULL, // 10^158
    0x91abb422ccb812efULL, // 10^159
    0xb616a12b7fe617aaULL, // 10^160
    0xe39c49765fdf9d95ULL, // 10^161
    0x8e41ade9fbebc27dULL, // 10^162
    0xb1d219647ae6b31cULL, // 10^163
    0xde469fbd99a05fe3ULL, // 10^164
    0x8aec23d680043beeULL, // 10^165
    0xada72ccc20054aeaULL, // 10^166
    0xd910f7ff28069da4ULL, // 10^167
    0x87aa9aff79042287ULL, // 10^168
    0xa99541bf57452b28ULL, // 10^169
    0xd3fa922f2d1675f2ULL, // 10^170
    0x847c9b5d7c2e09b7ULL, // 10^171
    0xa59bc234db398c25ULL, // 10^172
    0xcf02b2c21207ef2fULL, // 10^173
    0x8161afb94b44f57dULL, // 10^174
    0xa1ba1ba79e1632dcULL, // 10^175
    0xca28a291859bbf93ULL, // 10^176
    0xfcb2cb35e702af78ULL, // 10^177
    0x9defbf01b061adabULL, // 10^178
    0xc56baec21c7a1916ULL, // 10^179
    0xf6c69a72a3989f5cULL, // 10^180
    0x9a3c2087a63f6399ULL, // 10^181
    0xc0cb28a98fcf3c80ULL, // 10^182
    0xf0fdf2d3f3c30b9fULL, // 10^183
    0x969eb7c47859e744ULL, // 10^184
    0xbc4665b596706115ULL, // 10^185
    0xeb57ff22fc0c795aULL, // 10^186
    0x9316ff75dd87cbd8ULL, // 10^187
    0xb7dcbf5354e9beceULL, // 10^188
    0xe5d3ef282a242e82ULL, // 10^189
    0x8fa475791a569d11ULL, // 10^190
    0xb38d92d760ec4455ULL, // 10^191
    0xe070f78d3927556bULL, // 10^192
    0x8c469ab843b89563ULL, // 10^193
    0xaf58416654a6babbULL, // 10^194
    0xdb2e51bfe9d0696aULL, // 10^195
    0x88fcf317f22241e2ULL, // 10^196
    0xab3c2fddeeaad25bULL, // 10^197
    0xd60b3bd56a5586f2ULL, // 10^198
    0x85c7056562757457ULL, // 10^199
    0xa738c6bebb12d16dULL, // 10^200
    0xd106f86e69d785c8ULL, // 10^201
    0x82a45b450226b39dULL, // 10^202
    0xa34d721642b06084ULL, // 10^203
    0xcc20ce9bd35c78a5ULL, // 10^204
    0xff290242c83396ceULL, // 10^205
    0x9f79a169bd203e41ULL, // 10^206
    0xc75809c42c684dd1ULL, // 10^207
    0xf92e0c3537826146ULL, // 10^208
    0x9bbcc7a142b17cccULL, // 10^209
    0xc2abf989935ddbfeULL, // 10^210
    0xf356f7ebf83552feULL, // 10^211
    0x98165af37b2153dfULL, // 10^212
    0xbe1bf1b059e9a8d6ULL, // 10^213
    0xeda2ee1c7064130cULL, // 10^214
    0x9485d4d1c63e8be8ULL, // 10^215
    0xb9a74a0637ce2ee1ULL, // 10^216
    0xe8111c87c5c1ba9aULL, // 10^217
    0x910ab1d4db9914a0ULL, // 10^218
    0xb54d5e4a127f59c8ULL, // 10^219
    0xe2a0b5dc971f303aULL, // 10^220
    0x8da471a9de737e24ULL, // 10^221
    0xb10d8e1456105dadULL, // 10^222
    0xdd50f1996b947519ULL, // 10^223
    0x8a5296ffe33cc930ULL, // 10^224
    0xace73cbfdc0bfb7bULL, // 10^225
    0xd8210befd30efa5aULL, // 10^226
    0x8714a775e3e95c78ULL, // 10^227
    0xa8d9d1535ce3b396ULL, // 10^228
    0xd31045a8341ca07cULL, // 10^229
    0x83ea2b892091e44eULL, // 10^230
    0xa4e4b66b68b65d61ULL, // 10^231
    0xce1de40642e3f4b9ULL, // 10^232
    0x80d2ae83e9ce78f4ULL, // 10^233
    0xa1075a24e4421731ULL, // 10^234
    0xc94930ae1d529cfdULL, // 10^235
    0xfb9b7cd9a4a7443cULL, // 10^236
    0x9d412e0806e88aa6ULL, // 10^237
    0xc491798a08a2ad4fULL, // 10^238
    0xf5b5d7ec8acb58a3ULL, // 10^239
    0x9991a6f3d6bf1766ULL, // 10^240
    0xbff610b0cc6edd3fULL, // 10^241
    0xeff394dcff8a948fULL, // 10^242
    0x95f83d0a1fb69cd9ULL, // 10^243
    0xbb764c4ca7a44410ULL, // 10^244
    0xea53df5fd18d5514ULL, // 10^245
    0x92746b9be2f8552cULL, // 10^246
    0xb7118682dbb66a77ULL, // 10^247
    0xe4d5e82392a40515ULL, // 10^248
    0x8f05b1163ba6832dULL, // 10^249
    0xb2c71d5bca9023f8ULL, // 10^250
    0xdf78e4b2bd342cf7ULL, // 10^251
    0x8bab8eefb6409c1aULL, // 10^252
    0xae9672aba3d0c321ULL, // 10^253
    0xda3c0f568cc4f3e9ULL, // 10^254
    0x8865899617fb1871ULL, // 10^255
    0xaa7eebfb9df9de8eULL, // 10^256
    0xd51ea6fa85785631ULL, // 10^257
    0x8533285c936b35dfULL, // 10^258
    0xa67ff273b8460357ULL, // 10^259
    0xd01fef10a657842cULL, // 10^260
    0x8213f56a67f6b29cULL, // 10^261
    0xa298f2c501f45f43ULL, // 10^262
    0xcb3f2f7642717713ULL, // 10^263
    0xfe0efb53d30dd4d8ULL, // 10^264
    0x9ec95d1463e8a507ULL, // 10^265
    0xc67bb4597ce2ce49ULL, // 10^266
    0xf81aa16fdc1b81dbULL, // 10^267
    0x9b10a4e5e9913129ULL, // 10^268
    0xc1d4ce1f63f57d73ULL, // 10^269
    0xf24a01a73cf2dcd0ULL, // 10^270
    0x976e41088617ca02ULL, // 10^271
    0xbd49d14aa79dbc82ULL, // 10^272
    0xec9c459d51852ba3ULL, // 10^273
    0x93e1ab8252f33b46ULL, // 10^274
    0xb8da1662e7b00a17ULL, // 10^275
    0xe7109bfba19c0c9dULL, // 10^276
    0x906a617d450187e2ULL, // 10^277
    0xb484f9dc9641e9dbULL, // 10^278
    0xe1a63853bbd26451ULL, // 10^279
    0x8d07e33455637eb3ULL, // 10^280
    0xb049dc016abc5e60ULL, // 10^281
    0xdc5c5301c56b75f7ULL, // 10^282
    0x89b9b3e11b6329bbULL, // 10^283
    0xac2820d9623bf429ULL, // 10^284
    0xd732290fbacaf134ULL, // 10^285
    0x867f59a9d4bed6c0ULL, // 10^286
    0xa81f301449ee8c70ULL, // 10^287
    0xd226fc195c6a2f8cULL, // 10^288
    0x83585d8fd9c25db8ULL, // 10^289
    0xa42e74f3d032f526ULL, // 10^290
    0xcd3a1230c43fb26fULL, // 10^291
    0x80444b5e7aa7cf85ULL, // 10^292
    0xa0555e361951c367ULL, // 10^293
    0xc86ab5c39fa63441ULL, // 10^294
    0xfa856334878fc151ULL, // 10^295
    0x9c935e00d4b9d8d2ULL, // 10^296
    0xc3b8358109e84f07ULL, // 10^297
    0xf4a642e14c6262c9ULL, // 10^298
    0x98e7e9cccfbd7dbeULL, // 10^299
    0xbf21e44003acdd2dULL, // 10^300
    0xeeea5d5004981478ULL, // 10^301
    0x95527a5202df0ccbULL, // 10^302
    0xbaa718e68396cffeULL, // 10^303
    0xe950df20247c83fdULL, // 10^304
    0x91d28b7416cdd27eULL, // 10^305
    0xb6472e511c81471eULL, // 10^306
    0xe3d8f9e563a198e5ULL, // 10^307
    0x8e679c2f5e44ff8fULL, // 10^308
    0xb201833b35d63f73ULL, // 10^309
    0xde81e40a034bcf50ULL, // 10^310
    0x8b112e86420f6192ULL, // 10^311
    0xadd57a27d29339f6ULL, // 10^312
    0xd94ad8b1c7380874ULL, // 10^313
    0x87cec76f1c830549ULL, // 10^314
    0xa9c2794ae3a3c69bULL, // 10^315
    0xd433179d9c8cb841ULL, // 10^316
    0x849feec281d7f329ULL, // 10^317
    0xa5c7ea73224deff3ULL, // 10^318
    0xcf39e50feae16bf0ULL, // 10^319
    0x81842f29f2cce376ULL, // 10^320
    0xa1e53af46f801c53ULL, // 10^321
    0xca5e89b18b602368ULL, // 10^322
    0xfcf62c1dee382c42ULL, // 10^323
    0x9e19db92b4e31ba9ULL, // 10^324
    0xc5a05277621be294ULL, // 10^325
    0xf70867153aa2db39ULL, // 10^326
    0x9a65406d44a5c903ULL, // 10^327
    0xc0fe908895cf3b44ULL, // 10^328
    0xf13e34aabb430a15ULL, // 10^329
    0x96c6e0eab509e64dULL, // 10^330
    0xbc789925624c5fe1ULL, // 10^331
    0xeb96bf6ebadf77d9ULL, // 10^332
    0x933e37a534cbaae8ULL, // 10^333
    0xb80dc58e81fe95a1ULL, // 10^334
    0xe61136f2227e3b0aULL, // 10^335
    0x8fcac257558ee4e6ULL, // 10^336
    0xb3bd72ed2af29e20ULL, // 10^337
    0xe0accfa875af45a8ULL, // 10^338
    0x8c6c01c9498d8b89ULL, // 10^339
    0xaf87023b9bf0ee6bULL, // 10^340
    0xdb68c2ca82ed2a06ULL, // 10^341
    0x892179be91d43a44ULL, // 10^342
};

static const int16_t cjsonx_eisel_lemire_exp[] = {
    -1220,
    -1216,
    -1213,
    -1210,
    -1206,
    -1203,
    -1200,
    -1196,
    -1193,
    -1190,
    -1186,
    -1183,
    -1180,
    -1176,
    -1173,
    -1170,
    -1166,
    -1163,
    -1160,
    -1156,
    -1153,
    -1150,
    -1146,
    -1143,
    -1140,
    -1136,
    -1133,
    -1130,
    -1127,
    -1123,
    -1120,
    -1117,
    -1113,
    -1110,
    -1107,
    -1103,
    -1100,
    -1097,
    -1093,
    -1090,
    -1087,
    -1083,
    -1080,
    -1077,
    -1073,
    -1070,
    -1067,
    -1063,
    -1060,
    -1057,
    -1053,
    -1050,
    -1047,
    -1043,
    -1040,
    -1037,
    -1034,
    -1030,
    -1027,
    -1024,
    -1020,
    -1017,
    -1014,
    -1010,
    -1007,
    -1004,
    -1000,
    -997,
    -994,
    -990,
    -987,
    -984,
    -980,
    -977,
    -974,
    -970,
    -967,
    -964,
    -960,
    -957,
    -954,
    -950,
    -947,
    -944,
    -940,
    -937,
    -934,
    -931,
    -927,
    -924,
    -921,
    -917,
    -914,
    -911,
    -907,
    -904,
    -901,
    -897,
    -894,
    -891,
    -887,
    -884,
    -881,
    -877,
    -874,
    -871,
    -867,
    -864,
    -861,
    -857,
    -854,
    -851,
    -847,
    -844,
    -841,
    -838,
    -834,
    -831,
    -828,
    -824,
    -821,
    -818,
    -814,
    -811,
    -808,
    -804,
    -801,
    -798,
    -794,
    -791,
    -788,
    -784,
    -781,
    -778,
    -774,
    -771,
    -768,
    -764,
    -761,
    -758,
    -754,
    -751,
    -748,
    -744,
    -741,
    -738,
    -735,
    -731,
    -728,
    -725,
    -721,
    -718,
    -715,
    -711,
    -708,
    -705,
    -701,
    -698,
    -695,
    -691,
    -688,
    -685,
    -681,
    -678,
    -675,
    -671,
    -668,
    -665,
    -661,
    -658,
    -655,
    -651,
    -648,
    -645,
    -642,
    -638,
    -635,
    -632,
    -628,
    -625,
    -622,
    -618,
    -615,
    -612,
    -608,
    -605,
    -602,
    -598,
    -595,
    -592,
    -588,
    -585,
    -582,
    -578,
    -575,
    -572,
    -568,
    -565,
    -562,
    -558,
    -555,
    -552,
    -549,
    -545,
    -542,
    -539,
    -535,
    -532,
    -529,
    -525,
    -522,
    -519,
    -515,
    -512,
    -509,
    -505,
    -502,
    -499,
    -495,
    -492,
    -489,
    -485,
    -482,
    -479,
    -475,
    -472,
    -469,
    -465,
    -462,
    -459,
    -455,
    -452,
    -449,
    -446,
    -442,
    -439,
    -436,
    -432,
    -429,
    -426,
    -422,
    -419,
    -416,
    -412,
    -409,
    -406,
    -402,
    -399,
    -396,
    -392,
    -389,
    -386,
    -382,
    -379,
    -376,
    -372,
    -369,
    -366,
    -362,
    -359,
    -356,
    -353,
    -349,
    -346,
    -343,
    -339,
    -336,
    -333,
    -329,
    -326,
    -323,
    -319,
    -316,
    -313,
    -309,
    -306,
    -303,
    -299,
    -296,
    -293,
    -289,
    -286,
    -283,
    -279,
    -276,
    -273,
    -269,
    -266,
    -263,
    -259,
    -256,
    -253,
    -250,
    -246,
    -243,
    -240,
    -236,
    -233,
    -230,
    -226,
    -223,
    -220,
    -216,
    -213,
    -210,
    -206,
    -203,
    -200,
    -196,
    -193,
    -190,
    -186,
    -183,
    -180,
    -176,
    -173,
    -170,
    -166,
    -163,
    -160,
    -157,
    -153,
    -150,
    -147,
    -143,
    -140,
    -137,
    -133,
    -130,
    -127,
    -123,
    -120,
    -117,
    -113,
    -110,
    -107,
    -103,
    -100,
    -97,
    -93,
    -90,
    -87,
    -83,
    -80,
    -77,
    -73,
    -70,
    -67,
    -63,
    -60,
    -57,
    -54,
    -50,
    -47,
    -44,
    -40,
    -37,
    -34,
    -30,
    -27,
    -24,
    -20,
    -17,
    -14,
    -10,
    -7,
    -4,
    0,
    3,
    6,
    10,
    13,
    16,
    20,
    23,
    26,
    30,
    33,
    36,
    39,
    43,
    46,
    49,
    53,
    56,
    59,
    63,
    66,
    69,
    73,
    76,
    79,
    83,
    86,
    89,
    93,
    96,
    99,
    103,
    106,
    109,
    113,
    116,
    119,
    123,
    126,
    129,
    132,
    136,
    139,
    142,
    146,
    149,
    152,
    156,
    159,
    162,
    166,
    169,
    172,
    176,
    179,
    182,
    186,
    189,
    192,
    196,
    199,
    202,
    206,
    209,
    212,
    216,
    219,
    222,
    226,
    229,
    232,
    235,
    239,
    242,
    245,
    249,
    252,
    255,
    259,
    262,
    265,
    269,
    272,
    275,
    279,
    282,
    285,
    289,
    292,
    295,
    299,
    302,
    305,
    309,
    312,
    315,
    319,
    322,
    325,
    328,
    332,
    335,
    338,
    342,
    345,
    348,
    352,
    355,
    358,
    362,
    365,
    368,
    372,
    375,
    378,
    382,
    385,
    388,
    392,
    395,
    398,
    402,
    405,
    408,
    412,
    415,
    418,
    422,
    425,
    428,
    431,
    435,
    438,
    441,
    445,
    448,
    451,
    455,
    458,
    461,
    465,
    468,
    471,
    475,
    478,
    481,
    485,
    488,
    491,
    495,
    498,
    501,
    505,
    508,
    511,
    515,
    518,
    521,
    524,
    528,
    531,
    534,
    538,
    541,
    544,
    548,
    551,
    554,
    558,
    561,
    564,
    568,
    571,
    574,
    578,
    581,
    584,
    588,
    591,
    594,
    598,
    601,
    604,
    608,
    611,
    614,
    617,
    621,
    624,
    627,
    631,
    634,
    637,
    641,
    644,
    647,
    651,
    654,
    657,
    661,
    664,
    667,
    671,
    674,
    677,
    681,
    684,
    687,
    691,
    694,
    697,
    701,
    704,
    707,
    711,
    714,
    717,
    720,
    724,
    727,
    730,
    734,
    737,
    740,
    744,
    747,
    750,
    754,
    757,
    760,
    764,
    767,
    770,
    774,
    777,
    780,
    784,
    787,
    790,
    794,
    797,
    800,
    804,
    807,
    810,
    813,
    817,
    820,
    823,
    827,
    830,
    833,
    837,
    840,
    843,
    847,
    850,
    853,
    857,
    860,
    863,
    867,
    870,
    873,
    877,
    880,
    883,
    887,
    890,
    893,
    897,
    900,
    903,
    907,
    910,
    913,
    916,
    920,
    923,
    926,
    930,
    933,
    936,
    940,
    943,
    946,
    950,
    953,
    956,
    960,
    963,
    966,
    970,
    973,
    976,
    980,
    983,
    986,
    990,
    993,
    996,
    1000,
    1003,
    1006,
    1009,
    1013,
    1016,
    1019,
    1023,
    1026,
    1029,
    1033,
    1036,
    1039,
    1043,
    1046,
    1049,
    1053,
    1056,
    1059,
    1063,
    1066,
    1069,
    1073,
};

#endif // CJSONX_EISEL_LEMIRE_H
