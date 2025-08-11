from distutils.core import setup

setup(
    name='dejaview',
    packages=[
        'dejaview',
        'dejaview.batch_trace_processor',
        'dejaview.common',
        'dejaview.trace_processor',
        'dejaview.trace_uri_resolver',
    ],
    package_data={'dejaview.trace_processor': ['*.descriptor']},
    include_package_data=True,
    version='0.10.0',
    license='apache-2.0',
    description='Python API for DejaView\'s Trace Processor',
    author='DejaView',
    author_email='dejaview-pypi@google.com',
    url='https://perfetto.dev/',
    download_url='https://github.com/google/perfetto/archive/refs/tags/v30.0.tar.gz',
    keywords=['trace processor', 'tracing', 'dejaview'],
    install_requires=[
        'protobuf',
    ],
    classifiers=[
        'Development Status :: 3 - Alpha',
        'License :: OSI Approved :: Apache Software License',
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.5",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
    ],
)
